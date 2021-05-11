from parlai.scripts.display_model import DisplayModel
from parlai.scripts.train_model import TrainModel
from parlai.core.teachers import register_teacher, DialogTeacher
from parlai.core.agents import register_agent, Agent
import os
from sys import argv
import boto3
from botocore.client import Config
from botocore.exceptions import NoCredentialsError
import shutil



directory = "poly-encoder"
if os.path.exists(directory):
    shutil.rmtree(directory)

ACCESS_KEY = "AKIAtEpiGWUcQIelPRlD1Pi6xQ"
SECRET_KEY = "YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK"
host = "http://192.168.2.102"
bucket_name = 'testing'
local_folder = "C:\\Users\\Administrator\\Desktop\\ai\\ParlAI\\training\\poly-encoder"
'''if os.path.isdir(local_folder):
    shutil.rmtree(local_folder)'''

#local_folder, s3_folder = argv[1:3]
walks = os.walk(local_folder)
# Function to upload to s3
def upload_to_s3(bucket, local_file, s3_file):
    """local_file, s3_file can be paths"""
    s3 = boto3.client('s3', endpoint_url= host,aws_access_key_id=ACCESS_KEY,
                      aws_secret_access_key=SECRET_KEY,config=Config(signature_version='s3v4'),
                         region_name='US')
    print('  Uploading ' +local_file + ' as ' + bucket + '\\' +s3_file)
    try:
        s3.upload_file(local_file, bucket, s3_file)
        print('  '+s3_file + ": Upload Successful")
        print('  ---------')
        return True
    except NoCredentialsError:
        print("Credentials not available")
        return False

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


DisplayModel.main(task='poly_teacher', model='poly')
TrainModel.main(
    model='seq2seq',
    model_file='poly-encoder/model',
    dict_file='zoo:dodecadialogue/empathetic_dialogues_ft/model.dicts',
    task='poly_teacher',
    batchsize=3,
    validation_every_n_secs=10,
    max_train_time=60,

)

DisplayModel.main(
    task='poly_teacher',
    model_file='poly-encoder/model',
    num_examples=6,
)
for source, dirs, files in walks:
        print('Directory: ' + source)
        for filename in files:
            # construct the full local path
            local_file = os.path.join(source, filename)
            # construct the full Dropbox path
            relative_path = os.path.relpath(local_file, local_folder)
            print(relative_path)
            s3_file = relative_path
            # Invoke upload function
            upload_to_s3(bucket_name, local_file, s3_file)
