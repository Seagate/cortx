#!/usr/bin/env python3

# Copyright (c) Facebook, Inc. and its affiliates.
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
"""
Basic script which allows local human keyboard input to talk to a trained model.

## Examples

```shell
parlai interactive -m drqa -mf "models:drqa/squad/model"
```

When prompted, enter something like: `Bob is Blue.\\nWhat is Bob?`

Input is often model or task specific, but in drqa, it is always
`context '\\n' question`.
"""
from parlai.core.params import ParlaiParser
from parlai.core.agents import create_agent
from parlai.core.worlds import create_task
from parlai.core.script import ParlaiScript, register_script
from parlai.utils.world_logging import WorldLogger
from local_human import LocalHumanAgent
import parlai.utils.logging as logging
import threading
import random
from flask import Flask, render_template, request
from waiting import wait
import os
import json
from parlai.scripts.train_model import TrainModel
from parlai.core.teachers import register_teacher, DialogTeacher
from parlai.core.agents import register_agent, Agent
import shutil
import boto3
from botocore.client import Config
from botocore.exceptions import NoCredentialsError
import shutil


ACCESS_KEY = "AKIAtEpiGWUcQIelPRlD1Pi6xQ"
SECRET_KEY = "YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK"
host = "http://192.168.2.102"
bucket_name = 'testing'
local_folder = "C:\\Users\\Administrator\\Desktop\\ai\\ParlAI\\training\\poly-encoder"
'''if os.path.isdir(local_folder):
    shutil.rmtree(local_folder)'''

#local_folder, s3_folder = argv[1:3]
walks = os.walk(local_folder)
# Function to download from s3.
def download_from_s3(bucket, local_file, s3_file):
    """local_file, s3_file can be paths"""
    s3 = boto3.client('s3', endpoint_url= host,aws_access_key_id=ACCESS_KEY,
                      aws_secret_access_key=SECRET_KEY,config=Config(signature_version='s3v4'),
                         region_name='US')
    print('  Downloding ' + s3_file + ' as ' + bucket + '\\' +local_file)
    try:
        s3.download_file(bucket, s3_file, local_file)
        print('  '+s3_file + ": download Successful")
        print('  ---------')
        return True
    except NoCredentialsError:
        print("Credentials not available")
        return False
for source, dirs, files in walks:
        print('Directory: ' + source)
        for filename in files:
            # construct the full local path
            local_file = os.path.join(source, filename)
            # construct the full Dropbox path
            relative_path = os.path.relpath(local_file, local_folder)
            print(relative_path)
            s3_file = relative_path
            # Invoke download function
            
            download_from_s3(bucket_name, local_file, s3_file)



@register_agent("poly")
class PolyAgent(Agent):
    @classmethod
    def add_cmdline_args(cls, parser, partial_opt):
        parser.add_argument('--name', type=str, default='Metell', help="The agent's name.")
        return parser

    def __init__(self, opt, shared=None):
        # similar to the teacher, we have the Opt and the shared memory objects!
        super().__init__(opt, shared)
        self.id = 'PolyAgent'
        self.name = opt['name']

    def observe(self, observation):
        # Gather the last word from the other user's input
        words = observation.get('text', '').split()
        if words:
            self.last_word = words[-1]
        else:
            self.last_word = "stranger!"

    def act(self):
        # Always return a string like this.
        return {
            'id': self.id,
            'text': f"Hello {self.last_word}, I'm {self.name}",
        }


@register_teacher("poly_teacher")
class PolyTeacher(DialogTeacher):
    def __init__(self, opt, shared=None):
        # opt is the command line arguments.

        # What is this shared thing?
        # We make many copies of a teacher, one-per-batchsize. Shared lets us store

        # We just need to set the "datafile".  This is boilerplate, but differs in many teachers.
        # The "datafile" is the filename where we will load the data from. In this case, we'll set it to
        # the fold name (train/valid/test) + ".txt"
        # opt['datafile'] = opt['datatype'].split(':')[0] + ".txt"
        opt['datafile'] = 'train.txt'
        print("__init__: ", opt['datafile'])
        super().__init__(opt, shared)

    def setup_data(self, datafile):
        # filename tells us where to load from.
        # We'll just use some hardcoded data, but show how you could read the filename here:
        print(f" ~~ Loading from {datafile} ~~ ")

        with open(datafile) as f:
            new_episode = True

            for line in f.readlines():
                splits = line.split('   ')

                if len(splits) == 3:
                    yield (splits[0].replace('text:', ''), splits[1].replace('labels:', '')), new_episode
                    new_episode = True
                else:
                    yield (splits[0].replace('text:', ''), splits[1].replace('labels:', '')), new_episode
                    new_episode = False



app = Flask(__name__)
app.static_folder = 'static'
input_file = "input.txt"
output_file = "output.txt"

def is_something_ready(file):
    file = os.path.isfile(file)

    if file is True:
        return True
    return False
def flask_func():
    @app.route("/")
    def home():
        return render_template("index.html")

    @app.route("/get")
    def get_bot_response():

        userText = request.args.get('msg')
        userid = request.args.get('userid')
        input_text = userText
        if os.path.isfile(input_file):
            os.remove("input.txt")
        if os.path.isfile(output_file):
            os.remove("output.txt")
   
        file = open("input.txt","w").write(input_text + "_" + userid)



        something = "output.txt"

        wait(lambda: is_something_ready(something), waiting_for="output.txt file to be ready")
        file = open("output.txt","r").read()
     
        result = str(file)


        return str(result)

    app.run(host='localhost', port=8080)




    @register_script('interactive', aliases=['i'])
    class Interactive(ParlaiScript):
        @classmethod
        def setup_args(cls):
            return setup_args()

        def run(self):
            return interactive(self.opt)
            print("interactive:",interactive(self.opt))

def main_func():

    def setup_args(parser=None):
        if parser is None:
            parser = ParlaiParser(
                True, True, 'Interactive chat with a model on the command line'
            )
        parser.add_argument('-d', '--display-examples', type='bool', default=False)
        parser.add_argument(
            '--display-prettify',
            type='bool',
            default=False,
            help='Set to use a prettytable when displaying '
            'examples with text candidates',
        )
        parser.add_argument(
            '--display-ignore-fields',
            type=str,
            default='label_candidates,text_candidates',
            help='Do not display these fields',
        )
        parser.add_argument(
            '-it',
            '--interactive-task',
            type='bool',
            default=True,
            help='Create interactive version of task',
        )
        parser.add_argument(
            '--outfile',
            type=str,
            default='',
            help='Saves a jsonl file containing all of the task examples and '
            'model replies. Set to the empty string to not save at all',
        )
        parser.add_argument(
            '--save-format',
            type=str,
            default='parlai',
            choices=['conversations', 'parlai'],
            help='Format to save logs in. conversations is a jsonl format, parlai is a text format.',
        )
        parser.set_defaults(interactive_mode=True, task='interactive')
        LocalHumanAgent.add_cmdline_args(parser)
        WorldLogger.add_cmdline_args(parser)
        return parser


    def interactive(opt):
        if isinstance(opt, ParlaiParser):
            logging.error('interactive should be passed opt not Parser')
            opt = opt.parse_args()

        # Create model and assign it to the specified task
        agent = create_agent(opt, requireModelExists=True)
        agent.opt.log()

        human_agent = LocalHumanAgent(opt)


          # set up world logger
        world_logger = WorldLogger(opt) if opt.get('outfile') else None
        world = create_task(opt, [human_agent, agent])





        # Show some example dialogs:
        while not world.epoch_done():
            world.parley()
            if world.epoch_done() or world.get_total_parleys() <= 0:
                # chat was reset with [DONE], [EXIT] or EOF
                if world_logger is not None:
                    world_logger.reset()
                continue

            if world_logger is not None:
                world_logger.log(world)
            if opt.get('display_examples'):
                print("---")
                print(world.display())

        if world_logger is not None:
            # dump world acts to file
            world_logger.write(opt['outfile'], world, file_format=opt['save_format'])



    @register_script('interactive', aliases=['i'])
    class Interactive(ParlaiScript):
        @classmethod
        def setup_args(cls):
            return setup_args()

        def run(self):
            return interactive(self.opt)
            print("interactive:",interactive(self.opt))

    random.seed(42)

    Interactive.main()



if __name__ == '__main__':
    t1 = threading.Thread(target=flask_func)
    t2 = threading.Thread(target=main_func)

    # starting thread 1
    t2.start()


    # starting thread 2
    t1.start()

    # wait until thread 1 is completely executed
    t2.join()
    # wait until thread 2 is completely executed
    t1.join()
    print("Done")