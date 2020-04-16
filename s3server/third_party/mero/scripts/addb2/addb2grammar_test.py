import unittest
import addb2grammar

# WARNING
# WARNING: Do not delete trailing whitespaces here! They are significatnt!
# WARNING

singular_defs=[
"""
* 2015-10-24-04:21:44.073956527 m0t1fs-create <4700000000000XXX:YYYYY>, mode: 100644, rc: 0
""",

"""
* 2015-10-24-04:21:44.073956527 m0t1fs-create <4700000000000000:10007>, mode: 100644, rc: 0
|         node             <11186d8bf0e34117:ab1897c062a22573>
|         thread           ffff88007407ca80, 7081
""",

# this one is triky, trailing tab after 'ast' and missing attr value
""" 
* 2019-08-09-09:24:38.824262804 /FOM states: Init -[Schedule]-> Ready  nr: 0 min: -1 max: 0 avg: 0.000000 dev: 0.000000 datum: 0  0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0
|         locality         0
|         ast              
"""
]


multiple_defs="""
* 2015-10-24-04:21:44.073956527 m0t1fs-create <4700000000000XXX:YYYYY>, mode: 100644, rc: 0
* 2015-10-24-04:21:44.073956527 m0t1fs-create <4700000000000000:10007>, mode: 100644, rc: 0
|         node             <11186d8bf0e34117:ab1897c062a22573>
|         thread           ffff88007407ca80, 7081
* 2015-04-20-14:36:13.687531192 alloc     size: 40,   addr: @0x7fd27c53eb20
|         node             <f3b62b87d9e642b2:96a4e0520cc5477b>
|         locality         1
|         thread           7fd28f5fe700
|         fom              @0x7fd1f804f710, 'IO fom' transitions: 13 phase: Zero-copy finish
|         stob-io-launch   2015-04-20-14:36:13.629431319, <200000000000003:10000>, count: 8, bvec-nr: 8, ivec-nr: 1, offset: 0
|         stob-io-launch   2015-04-20-14:36:13.666152841, <100000000adf11e:3>, count: 8, bvec-nr: 8, ivec-nr: 8, offset: 65536
* 2015-04-14-15:33:11.998165453 fom-descr service: <7300000000000001:0>, sender: c28baccf27e0001, req-opcode: Read request, rep-opcode: Read reply, local: false
|           :         0 |
|         1 :         0 |
|         3 :         0 |
|         5 :         0 |
|         7 :         0 |
|         9 :         0 |
|        11 :         0 |
|        13 :         0 |
|        15 :         0 |
|        17 :         0 |
|        19 :         0 |
|        21 :         0 |
|        23 :         0 |
|        25 :         0 |
|         node             <11186d8bf0e34117:ab1897c062a22573>
|         locality         3
|         thread           7f79e57fb700
|         ast
|         fom              @0x7f795008ed20, 'IO fom', transitions: 0, phase: 0
* 2019-08-09-09:24:38.824262804 /FOM states: Init -[Schedule]-> Ready  nr: 0 min: -1 max: 0 avg: 0.000000 dev: 0.000000 datum: 0  0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0 0: 0
|           :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         node             <a9be317d8e664514:b5d3bfe7e01fa1f8>
|         pid              26163
|         locality         0
|         thread           7fb6e4a0b700
|         ast              
* 2019-08-09-09:24:38.824264676 /XXX states: YYY -[Failed]-> ZZZ  nr: 0 min: -1 max: 0 avg: 0.000000
|           :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         0 :         0 | 
|         node             <aYbe3Z7d8e6645Z4:b5d3bfe7e0ZfaZf8>
|         pid              26163
|         locality         0
|         thread           7fb6e4a0b700
|         ast              
"""


class ADDB2GrammarTest(unittest.TestCase):
    def setUp(self):
        self.grammar = addb2grammar.Addb2Grammar()
        
    def test_singular(self):
        for t in singular_defs:
            res = self.grammar.parse(t)
            self.assertTrue(res.is_valid)
            
    def test_multiple(self):
        res = self.grammar.parse(multiple_defs)
        self.assertTrue(res.is_valid)

if __name__ == '__main__':
    unittest.main()
