import json
import requests
import os
import re
import glob

API_URL = "https://github.com/Seagate/"
response  = json.loads(requests.get("http://cortx-storage.colo.seagate.com/releases/opensource_builds/alex_report/config.json").text)
global_words = response["global_scope"]
repo_name=[]

for repo_list in response["repository"]:
  local_words = repo_list["local_scope"]
  repo_url= API_URL + repo_list["name"]
  repo_name.append(repo_list["name"])
  os.system("cd ./ && git clone "+ repo_url)

for x in repo_name:
  os.system("cp -r ./cortx-re/scripts/automation/alex/ ./ && cp -r ./cortx-re/scripts/automation/alex/alex_template.html ./")
  os.system("cp -r ./cortx-re/scripts/automation/alex/alexignore ./" + x + "/.alexignore")
  os.system("cd "+ "./" + x + " && " + "touch " + x + ".custom")
  os.system("cd "+ "./" + x + " && " + "grep -rnwo '.' --regexp='western digital\|hack\|trash\|garbage\|rubbish\|junk\|snowflake\|mero\|eos\|clovis\|5u84\|ees\|ecs\|Xyratex\|Fujitsu\|WD\|Samsung\|lyve cloud' >> " + x + ".custom")
  os.system("cd "+ "./" + x + " && " + "mv " + x + ".custom" + " ./")
  os.system("cd "+ "./" + x + " && " + "touch " + x + ".alex")
  os.system("cd "+ "./" + x + " && " + "alex . >>" + x + ".alex" + " 2>&1")
  os.system("cd "+ "./" + x + " && " + "mv " + x + ".alex" + " ./")
  os.system("mv ./" + x + "/" + x + ".alex" + " ./")
  os.system("mv ./" + x + "/" + x + ".custom" + " ./")
  os.system("export DATE_NOW=$(date +'%Y-%m-%d %T') && python3 alex/alex.py -f" + x + ".alex")

os.system("export ALEX_PATH=./$(date +'%Y-%m-%d %T') && export FILE_NAME=alex.pickle &&  python3 alex/alex_pickle.py")

html_files = glob.glob("cortx*.html")
#print (html_files)
alex_report_status = {}
for file in html_files:
    with open(file,'r') as f_obj:
       cont = f_obj.read()
    counts = dict()
    result = re.search(r'Component\s*Name</td><td>([^>]+?)<', cont)
    repo = result.group(1)

    pattern = re.compile(r'class="word_width">([^>]+)<')

    for match in pattern.finditer(cont):
       word = match.group(1)
       word = word.replace("`", "")
       if word in counts:
          counts[word] += 1
       else:
          counts[word] = 1
    alex_report_status[repo] = counts["Word"]

alex=alex_report_status
