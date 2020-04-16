/*
 * COPYRIGHT 2017 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 29-May-2017
 */

#include <assert.h>
#include <yaml-cpp/yaml.h>
#include <iostream>

#include "s3_clovis_layout.h"
#include "s3_log.h"

S3ClovisLayoutMap* S3ClovisLayoutMap::instance = NULL;

S3ClovisLayoutMap::S3ClovisLayoutMap() {
  best_layout_id = 11;
  obj_size_cap = 0;
  layout_id_cap = -1;
  layout_map[1] = 4096 * 1;  // bytes
  layout_map[2] = 4096 * 2;  // 8192 bytes
  layout_map[3] = 4096 * 4;
  layout_map[4] = 4096 * 8;
  layout_map[5] = 4096 * 16;
  layout_map[6] = 4096 * 32;
  layout_map[7] = 4096 * 64;
  layout_map[8] = 4096 * 128;
  layout_map[9] = 4096 * 256;
  layout_map[10] = 4096 * 512;
  layout_map[11] = 4096 * 1024;
  layout_map[12] = 4096 * 2048;
  layout_map[13] = 4096 * 4096;
  layout_map[14] = 4096 * 8192;
}

bool S3ClovisLayoutMap::load_layout_recommendations(std::string filename) {
  s3_log(S3_LOG_DEBUG, "", "Entering with filename = %s\n", filename.c_str());
  try {
    YAML::Node root_node = YAML::LoadFile(filename);
    if (root_node.IsNull()) {
      return false;  // File Not Found?
    }

    YAML::Node mapping_node = root_node["S3_OBJ_SIZE_MAPPING"];
    if (mapping_node.IsNull()) {
      return false;
    }

    // Fetch mappings
    for (YAML::const_iterator it = mapping_node.begin();
         it != mapping_node.end(); ++it) {
      YAML::Node node = *it;
      if (node.size() == 2) {
        YAML::Node::const_iterator sub_it = node.begin();
        size_t obj_size = 0;
        int layout_id = 0;
        bool cap_entry = false;
        bool best_layout_id_entry = false;
        for (YAML::Node::const_iterator sub_it = node.begin();
             sub_it != node.end(); sub_it++) {
          if (sub_it->first.as<std::string>() == "USE_LAYOUT_ID") {
            layout_id = sub_it->second.as<int>();
          } else if (sub_it->first.as<std::string>() == "UP_TO_OBJ_SIZE") {
            obj_size = sub_it->second.as<int>();
          } else if (sub_it->first.as<std::string>() == "ABOVE_OBJ_SIZE") {
            obj_size = sub_it->second.as<int>();
            cap_entry = true;
          } else if (sub_it->first.as<std::string>() == "BEST_LAYOUT_ID") {
            best_layout_id = sub_it->second.as<int>();
            best_layout_id_entry = true;
          } else {
            fprintf(stderr, "Incorrect file format: %s\n", filename.c_str());
            return false;
          }
        }  // inner for
        if (cap_entry) {
          obj_size_cap = obj_size;
          layout_id_cap = layout_id;
        } else if (!best_layout_id_entry) {
          obj_layout_map[obj_size] = layout_id;
        }
      } else {
        fprintf(stderr, "Incorrect file format: %s\n", filename.c_str());
        return false;
      }
    }  // outer for
    if (obj_size_cap == 0 || layout_id_cap == -1) {
      fprintf(stderr,
              "Max cap missing for object size and layout mapping: %s\n",
              filename.c_str());
      return false;
    }
  } catch (const YAML::RepresentationException& e) {
    fprintf(stderr, "%s:%d:YAML::RepresentationException caught: %s\n",
            __FILE__, __LINE__, e.what());
    fprintf(stderr, "%s:%d:Yaml file %s is incorrect\n", __FILE__, __LINE__,
            filename.c_str());
    return false;
  } catch (const YAML::ParserException& e) {
    fprintf(stderr, "%s:%d:YAML::ParserException caught: %s\n", __FILE__,
            __LINE__, e.what());
    fprintf(stderr, "%s:%d:Parsing Error in yaml file %s\n", __FILE__, __LINE__,
            filename.c_str());
    return false;
  } catch (const YAML::EmitterException& e) {
    fprintf(stderr, "%s:%d:YAML::EmitterException caught: %s\n", __FILE__,
            __LINE__, e.what());
    return false;
  } catch (YAML::Exception& e) {
    fprintf(stderr, "%s:%d:YAML::Exception caught: %s\n", __FILE__, __LINE__,
            e.what());
    return false;
  }
  return true;
}

int S3ClovisLayoutMap::get_best_layout_for_object_size() {
  return best_layout_id;
}

// Returns the <layout id and unit size> recommended for give object size.
int S3ClovisLayoutMap::get_layout_for_object_size(size_t obj_size) {
  s3_log(S3_LOG_DEBUG, "", "Entering with obj_size = %zu\n", obj_size);

  if (obj_size == 0 || obj_size <= obj_layout_map.begin()->first) {
    // obj_size is zero OR less than the smallest UP_TO_OBJ_SIZE
    s3_log(S3_LOG_DEBUG, "", "USE_LAYOUT_ID = %d\n",
           obj_layout_map.begin()->second);
    return obj_layout_map.begin()->second;  // layout
  }

  if (obj_size >= obj_size_cap) {
    s3_log(S3_LOG_DEBUG, "", "USE_LAYOUT_ID = %d\n", layout_id_cap);
    return layout_id_cap;
  }

  for (auto item = obj_layout_map.rbegin(); item != obj_layout_map.rend();
       ++item) {
    if (obj_size >= item->first) {
      s3_log(S3_LOG_DEBUG, "", "USE_LAYOUT_ID = %d\n", item->second);
      return item->second;  // layout
    }
  }

  // Redundant w.r.t first check in this method, but for sake of it
  s3_log(S3_LOG_DEBUG, "", "USE_LAYOUT_ID = %d\n",
         obj_layout_map.begin()->second);
  return obj_layout_map.begin()->second;
}
