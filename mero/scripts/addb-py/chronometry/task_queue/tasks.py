from config import huey
import os
from datetime import datetime
import plumbum

def io_workload_opt(conf_yaml, result_dir):
    lookup = {
        "mero_build" : "-b",
        "halon_build": "-d",
        "halon"      : "-e",
        "halon_facts": "-f",
        "git"        : "-g",
        "mero"       : "-m",
        "m0crate"    : "-w"
    }
    result = []
    for key in conf_yaml.keys():
        if key in lookup:
            iow_opt=" ".join([f"{x}={y}" for (x,y) in conf_yaml[key].items()])
            result.append(f"{lookup[key]}")
            result.append(f"{iow_opt}")
    result.append('-p')
    result.append(f'{result_dir}')
    return result

def run_scripts(conf, hooks, tid):
    if hooks in conf:
        for script in conf[hooks]:
            try:
                sc = plumbum.local[script]
                sc[tid] & plumbum.FG
            except plumbum.commands.processes.ProcessExecutionError:
                print(f"File {script} failed")
            except (FileNotFoundError,
                    plumbum.commands.processes.CommandNotFound) as e:
                print(f"File {script} not found")

def send_mail(to, status, tid):
    nl="\n"
    msg = f"Subject: Cluster task queue{nl}Task {tid} {status}"
    sendmail = plumbum.local["sendmail"]
    echo = plumbum.local["echo"]
    chain = echo[msg] | sendmail[to]
    try:
        chain()
    except:
        print(f"Couldn't send email to {to}")

def list_artifacts(path):
    arts = []
    for (dirpath, dirnames, filenames) in os.walk(path):
        for file in filenames:
            arts.append(f"{dirpath}/{file}")
    return arts

def pack_artifacts(path):
    tar = plumbum.local["tar"]
    tar[f"-cJvf {path}.tar.xz {path}".split(" ")] & plumbum.FG
    print(f"Rm path: {path}")
    rm = plumbum.local["rm"]
    rm[f"-rf {path}".split(" ")]()

@huey.task(context=True)
def io_workload_task(conf_opt, task):
    conf,opt = conf_opt
    current_task = {
        'task_id': task.id,
        'pid'    : os.getpid(),
        'args'   : conf_opt,
    }
    huey.put('current_task', current_task)
    path = f"{os.getcwd()}"
    dirname = f"result_{task.id}"
    result = {
        'conf'          : conf,
        'start_time'    : str(datetime.now()),
        'artifacts_dir' : f"{path}/{dirname}"
    }
    result.update(opt)

    send_mail(conf['notify']['email'], "started", task.id)
    run_scripts(conf['notify'], 'start_task_hooks', task.id)

    if conf['testmode']:
        sleep = plumbum.local["sleep"]
        sleep(5)
        result['status']='SUCCESS'
    else:
        io_workload = plumbum.local["../run_task"]
        try:
            io_workload[io_workload_opt(conf, result["artifacts_dir"])] & plumbum.FG
            result['status']='SUCCESS'
        except plumbum.commands.processes.ProcessExecutionError:
            result['status']='FAILED'

    result['finish_time'] = str(datetime.now())
    run_scripts(conf['notify'], 'finish_task_hooks', task.id)
    send_mail(conf['notify']['email'],
              f"finished, status {result['status']}", task.id)
    result['artifacts'] = list_artifacts(result["artifacts_dir"])
    pack_artifacts(result["artifacts_dir"])

    return result

@huey.post_execute()
def post_execute_hook(task, task_value, exc):
    if exc is not None:
        print(f'Task "{task.id}" failed with error: {exc}')
    # Current task finished - do cleanup
    huey.get('current_task')
