import os
import json
from elasticsearch import Elasticsearch
from elasticsearch.exceptions import ConflictError


class ElasticsearchConnector():
    """Class for connecting to Elastic Search Service"""

    def __init__(self, elastic_domain: str, elastic_port: str):
        """Initialize Elastic search service"""
        try:
            domain_str = elastic_domain + ":" + elastic_port
            self.es = Elasticsearch(elastic_domain)
            print(json.dumps(Elasticsearch.info(self.es), indent=4), "\n")
            self.index = "file_index"
            self.create_index_if_not_exists()
            # self.es.indices.delete(index="file_index")
        except Exception as err:
            print("Elasticsearch() ERROR:", err, "\n")
            self.es = None

    def create_index_if_not_exists(self):
        """ Create an index """
        if self.es.indices.exists(index=self.index) is False:
            mapping = {
                "settings": {
                    "number_of_shards": 1,
                    "number_of_replicas": 0,
                    "analysis": {
                        "analyzer": {
                            "edge_ngram_analyzer": {
                                "type": "custom",
                                "tokenizer": "edge_ngram_tokenizer",
                                "filter": [
                                    "lowercase"
                                ]
                            },
                            "lower_search_analyzer": {
                                "tokenizer": "standard",
                                "filter": [
                                    "lowercase"
                                ]
                            }
                        },
                        "tokenizer": {
                            "edge_ngram_tokenizer": {
                                "type": "edge_ngram",
                                "min_gram": 2,
                                "max_gram": 10,
                                "token_chars": [
                                    "letter",
                                    "digit"
                                ]
                            }
                        }
                    }
                },
                "mappings": {
                    "properties": {
                        "file_name": {
                            "type": "text",
                            "analyzer": "edge_ngram_analyzer",
                            "search_analyzer": "lower_search_analyzer"
                        },
                        "file_id": {
                            "type": "text"
                        },
                        "created": {
                            "type": "text"
                        },
                        "timestamp": {
                            "type": "text"
                        },
                        "mimetype": {
                            "type": "text"
                        },
                        "filetype": {
                            "type": "text"
                        },
                        "user_id": {
                            "type": "text"
                        },
                        "size": {
                            "type": "text"
                        }
                    }
                }
            }
            self.es.indices.create(index=self.index, body=mapping)

    def check_if_doc_exists(self, file_name: str):
        """Check if a document exists with that particular file_name
        """
        return self.es.exists(index=self.index, id=file_name)

    def create_doc(self, file_id, file_name, created, timestamp, mimetype, filetype, user_id, size):
        """Creates a new document in the index for elastic search"""
        try:
            # filename, file_extension = os.path.splitext(file_name)
            body = {
                "file_id": file_id, "file_name": file_name, "created": str(created), "timestamp": str(timestamp), "mimetype": mimetype, "filetype": filetype, "user_id": user_id, "size": str(size),
            }
            self.es.create(index=self.index, id=file_name, body=body)
            return True
        except ConflictError as e:
            print("Error is {}".format(e))
            return False

    def delete_doc(self, file_name):
        """Removes a document from the index"""
        if(self.check_if_doc_exists(file_name)):
            self.es.delete(index=self.index, id=file_name)

    def get_doc(self, file_name):
        pass

    def search(self, text: str):
        query = {"from": 0, "size": 20, "query": {
            "match": {"file_name": {"query": text, "operator": "and"}}}}
        res = self.es.search(index=self.index, body=query)
        return res["hits"]["hits"]

    def get_all_docs(self):
        """Get all docs in ES"""
        result = self.es.search(
            index=self.index,
            body={
                "query": {
                    "match_all": {}
                }
            }
        )
        print(result["hits"]["hits"])
        return result


if __name__ == "__main__":
    es = ElasticsearchConnector('http://localhost', '9200')
    # es.create_doc(file_id='F0200P2T50C', file_name='abdul-kalam-hindi-1.mp4', created=1619173844,
    #              timestamp=1619173844, mimetype='video/mp4', filetype='mp4', user_id='U01U4DV4C8J', size='1807463')
    # es.create_doc(file_id='F0200P2T50C', file_name='abc.png', created=1619173844,
    #              timestamp=1619173844, mimetype='jpeg/png', filetype='png', user_id='U01U4DV4C8J', size='1807463')
    # es.create_doc(file_id='F0200P2T50C', file_name='abcd.png', created=1619173844,
    #              timestamp=1619173844, mimetype='jpeg/png', filetype='png', user_id='U01U4DV4C8J', size='1807463')
    # es.create_doc(file_id='F0200P2T50C', file_name='abcd.csv', created=1619173844,
    #              timestamp=1619173844, mimetype='csv', filetype='csv', user_id='U01U4DV4C8J', size='1807463')

    # es.get_all_docs()
    # es.suggest_file('abcd')
    # es.delete_doc('P0ZBAHkB9Uw1REUt_g9t')
    # es.delete_doc('abdul-kalam-hindi-1.mp4')
    # es.delete_doc('abc.png')
    # es.delete_doc('abcd.png')
    # es.delete_doc('abcd.csv')
