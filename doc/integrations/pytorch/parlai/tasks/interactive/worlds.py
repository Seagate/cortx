#!/usr/bin/env python3

# Copyright (c) Facebook, Inc. and its affiliates.
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

from copy import deepcopy

from parlai.core.worlds import DialogPartnerWorld, validate
from parlai.core.message import Message
import subprocess
import os
import time

class InteractiveWorld(DialogPartnerWorld):
    """
    Simple interactive world involving just two agents talking.

    In more sophisticated worlds the environment could supply information, e.g. in
    tasks/convai2 both agents are given personas, so a world class should be written
    especially for those cases for given tasks.
    """

    def __init__(self, opt, agents, shared=None):
        super().__init__(opt, agents, shared)
        self.init_contexts(shared=shared)
        self.turn_cnt = 0

    def init_contexts(self, shared=None):
        """
        Override to load or instantiate contexts to be used to seed the chat.
        """
        pass

    def get_contexts(self):
        """
        Override to return a pair of contexts with which to seed the episode.

        This function will be called before the first turn of every episode.
        """
        return ['', '']

    def finalize_episode(self):
        print("CHAT DONE ")
        if not self.epoch_done():
            print("\n... preparing new chat... \n")

    def parley(self):



        """
        Agent 0 goes first.

        Alternate between the two agents.
        """
     
        if self.turn_cnt == 0:
            self.p1, self.p2 = self.get_contexts()

        acts = self.acts
        print("acts",acts)
        agents = self.agents
      

         
        if self.turn_cnt == 0 and self.p1 != '':
            # add the context on to the first message to agent 0
            context_act = Message(
                {'id': 'context', 'text': self.p1, 'episode_done': True}
            )
            agents[0].observe(validate(context_act))
        try:
            act = deepcopy(agents[0].act())
        except StopIteration:
            self.reset()
            self.finalize_episode()
            self.turn_cnt = 0
            return
        acts[0] = act
        if self.turn_cnt == 0 and self.p2 != '':
            # add the context on to the first message to agent 1
            context_act = Message(
                {'id': 'context', 'text': self.p2, 'episode_done': False}
            )
            agents[1].observe(validate(context_act))
        try:
            try:
                cache = dict() # caching to save the user_id
                def get_user_id(user_id):
                    print("getting user id")
                    if user_id not in cache:
                        agents_clone_human = deepcopy(agents[0].clone())
                        agents_clone_bot =deepcopy(agents[1].clone())
                        agents[0] =agents_clone_human
                        agents[1] = agents_clone_bot
                        print("new agent created")

                        cache[user_id] = user_id
                    return cache[user_id]
                file = open("input.txt","r").read()
                text = file.split("_")
                text = text[1]
                print("user_id_value",text)
                userid = get_user_id(text)

            

                
            except :
                print("None")


            
          
            agents[1].observe(validate(act))
            if os.path.isfile("input.txt"):
                os.remove("input.txt")

        
            print("client:",act)
            acts[1] = agents[1].act()
            print("agent:",acts[1])
            print("text only:",acts[1]['text'])
            file= open("output.txt","w").write(str(acts[1]['text']))
          
           
         
            agents[0].observe(validate(acts[1]))
            self.update_counters()
            self.turn_cnt += 1
            if act['episode_done']:
                
                self.finalize_episode()
                self.turn_cnt = 0
        except:
            #agents[1].observe(validate(act))
            
            if os.path.isfile("input.txt"):
                os.remove("input.txt")
            print("client:",act)
            acts[1] = {'id': 'ImageSeq2seq', 'episode_done': False, 'text': "I didn't get that"}
            print("agent:",acts[1])
            print("text only:",acts[1]['text'])
          
            

            
            agents[0].observe(validate(acts[1]))
            self.update_counters()
            self.turn_cnt += 1
            act = {'id': 'localHuman', 'episode_done': False, 'label_candidates': None, 'text': 'hi'}
            if act['episode_done']:
                
                self.finalize_episode()
                self.turn_cnt = 0
        

       