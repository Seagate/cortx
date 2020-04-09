"""trace class accepts a stream of incoming records (represented by 
   the record class) and produces the output in the form of an SVG image, 
   describing the stream.

   Some assumptions are made about the input stream:

       - all records are from the same process,

       - the time-stamps of incoming records are monotonically
         increasing. m0addb2dump output does not necessarily conform to this
         restriction. This can always be fixed by doing

             m0addb2dump -f | sort -k2.1,2.29 -s | m0addb2dump -d

       - if a trace does not contain all records produced by a Mero process from
         start-up to shutdown, certain corner cases (like re-use of the same
         memory address for a new fom), can result in an incorrect
         interpretation of a final portion of the trace.

    Output layout.

    The entirety of the output image is divided into vertical stripes,
    corresponding to the localities, divided by 10px-wide vertical lines . The
    number of localities is specified by the "loc_nr" parameter.

    Vertical axis corresponds to time (from top to bottom). Horizontal dashed
    lines mark time-steps with the granularity specified by the "step"
    parameter.

    In the area, corresponding to a locality, the foms, executed in this
    locality are represented. Each fom is represented as a rectangle with fixed
    width and with the height corresponding to the fom life-time. The left
    border of this rectangle is marked by a black line, other borders are
    transparent.

    The interior of a fom rectangle is divided into 3 vertical "lanes". The
    first lane is used for labels. When a fom is created, its type and address
    are written to the label lane. When the fom experiences a phase transition,
    the name of the new phase is written to the label lane.

    The second lane, represents phases, marked by different colours.

    The third lane contains states, marked by different colours.

    The line at the left border of fom area can be missing if the final state
    transition for the fom is missing from the log.

    If fom phase state machine doesn't have transition descriptions, the phase
    lane will be empty and phase labels will be missing.

    By specifying "starttime" and "duration" parameters, view area can be
    narrowed to produce more manageable images. When view area is narrowed, the
    entire trace is still processed, but the SVG elements that would fall outside
    of the visible image are omitted.

"""

import datetime
import svgwrite

class trace(object):
    def __init__(self, width, height, loc_nr, duration, starttime = None,
                 step = 100, outname = "out.svg", maxfom = 20, verbosity = 0,
                 label = True):
        self.timeformat = "%Y-%m-%d-%H:%M:%S.%f"
        if starttime != None:
            self.start = datetime.datetime.strptime(starttime, self.timeformat)
        else:
            self.start = None
        self.prep   = False
        self.label  = label
        self.width  = width
        self.height = height
        self.loc_nr = loc_nr
        self.usec   = duration * 1000000
        self.step   = step
        self.verb   = verbosity
        self.maxfom = maxfom
        self.out    = svgwrite.Drawing(outname, profile='full', \
                                       size = (str(width)  + "px",
                                               str(height) + "px"))
        self.lmargin     = width * 0.01
        self.reqhwidth   = width * 0.87
        self.lockwidth   = width * 0.01
        self.netwidth    = width * 0.05
        self.iowidth     = width * 0.05
        self.rmargin     = width * 0.01

        assert (self.lmargin + self.reqhwidth + self.lockwidth + self.netwidth +
                self.iowidth + self.rmargin == self.width)

        self.reqhstart   = self.lmargin
        self.lockstart   = self.reqhstart + self.reqhwidth
        self.netstart    = self.lockstart + self.lockwidth
        self.iostart     = self.netstart + self.netwidth

        self.loc_width   = self.reqhwidth / loc_nr
        self.loc_margin  = self.loc_width * 0.02
        self.fom_width   = (self.loc_width - 2*self.loc_margin) / self.maxfom
        self.maxlane     = 4
        self.lane_margin = self.fom_width * 0.10
        self.lane_width  = (self.fom_width - 2*self.lane_margin) / self.maxlane
        self.axis        = svgwrite.rgb(0, 0, 0, '%')
        self.locality    = []
        self.iomax       = 128
        self.iolane      = (self.iowidth - 300) / self.iomax
        self.iolane0     = self.iostart + 300
        self.iolast      = []
        self.netmax      = 128
        self.netlane     = (self.netwidth - 400) / self.netmax
        self.netlane0    = self.netstart + 400
        self.netlast     = []
        self.lockmax     = 32
        self.locklane    = (self.lockwidth - 300) / self.lockmax
        self.locklane0   = self.lockstart + 300
        self.locks       = {}
        self.processed   = 0
        self.reported    = 0
        self.textstep    = 15
        self.scribbles   = set()
        self.foms        = {}
        self.dash        = {
            "stroke"           : self.axis,
            "stroke_width"     : 1,
            "stroke_dasharray" :"1,1"
        }
        self.warnedlabel = False
        self.warnednet   = False
        self.warnedlock  = False
        self.warnedio    = False
        self.warnedfom   = 0
        for i in range(loc_nr):
            x = self.getloc(i)
            self.locality.append(locality(self, i))
            self.line((x, 0), (x, height), stroke = self.axis, stroke_width = 10)
            self.text("locality " + str(i), insert = (x + 10, 20))
        for _ in range(self.iomax):
            self.iolast.append(datetime.datetime(1970, 01, 01))
        for _ in range(self.netmax):
            self.netlast.append(datetime.datetime(1970, 01, 01))
        self.line((self.lockstart - 10, 0), (self.lockstart - 10, height),
                  stroke = self.axis, stroke_width = 10)
        self.text("lock", insert = (self.lockstart + 10, 20))
        self.line((self.netstart - 10, 0), (self.netstart - 10, height),
                  stroke = self.axis, stroke_width = 10)
        self.text("net", insert = (self.netstart + 10, 20))
        self.line((self.iostart - 10, 0), (self.iostart - 10, height),
                  stroke = self.axis, stroke_width = 10)
        self.text("io", insert = (self.iostart + 10, 20))

    def done(self):
        self.out.save()

    def fomadd(self, fom):
        self.locality[fom.getloc()].fomadd(fom)

    def fomdel(self, fom):
        self.locality[fom.getloc()].fomdel(fom)

    def getloc(self, idx):
        return self.reqhstart + self.loc_width * idx

    def getlane(self, fom, lane):
        assert 0 <= lane and lane < self.maxlane
        return self.getloc(fom.loc.idx) + self.loc_margin + \
            self.fom_width * fom.loc_idx + self.lane_margin + \
            self.lane_width * lane

    def getpos(self, stamp):
        interval = stamp - self.start
        usec = interval.microseconds + (interval.seconds +
                                        interval.days * 24 * 3600) * 1000000
        return self.height * usec / self.usec

    def fomfind(self, rec):
        addr = rec.get("fom")
        f = self.foms.get(addr)
        if f == None:
            f = fom()
            f.time   = self.start
            f.params = [None, None, None, None, None, rec.ctx["fom"][1]]
            f.ctx    = rec.ctx
            f.done(self)
        return f

    def getcolour(self, str):
        seed = str + "^" + str
        red   = hash(seed + "r") % 90
        green = hash(seed + "g") % 90
        blue  = hash(seed + "b") % 90
        return svgwrite.rgb(red, green, blue, '%')
        
    def fomcolour(self, fom):
        return self.getcolour(fom.phase)

    def fomrect(self, fom, lane, start, end):
        start  = self.getpos(start)
        height = self.getpos(end) - start
        lane   = self.getlane(fom, lane)
        return { "insert": (lane, start), "size": (self.lane_width, height) }

    def statecolour(self, fom):
        state = fom.state
        if state == "Init":
            return svgwrite.rgb(100, 100, 0, '%')
        elif state == "Ready":
            return svgwrite.rgb(100, 0, 0, '%')
        elif state == "Running":
            return svgwrite.rgb(0, 100, 0, '%')
        elif state == "Waiting":
            return svgwrite.rgb(0, 0, 100, '%')
        else:
            return svgwrite.rgb(10, 10, 10, '%')

    def rect(self, **kw):
        y = kw["insert"][1]
        h = kw["size"][1]
        if y + h >= 0 and y < self.height:
            self.out.add(self.out.rect(**kw))

    def line(self, start, end, **kw):
        if end[1] >= 0 and start[1] < self.height:
            self.out.add(self.out.line(start, end, **kw))

    def tline(self, start, end, **kw):
        if self.label:
            self.line(start, end, **kw)

    def text(self, text, connect = False, force = False, **kw):
        x = int(kw["insert"][0])
        y0 = y = int(kw["insert"][1]) // self.textstep * self.textstep
        if not self.label and not force:
            return (x, y)
        if y >= 0 and y < self.height:
            i = 0
            while (x, y // self.textstep) in self.scribbles:
                y += self.textstep
                if i > 30:
                    if not self.warnedlabel:
                        print "Labels are overcrowded. Increase image height."
                        self.warnedlabel = True
                    break
                i += 1
            kw["insert"] = (x + 10, y)
            kw["font_family"] = "Courier"
            self.out.add(self.out.text(text, **kw))
            if connect:
                self.line((x, y0), (x + 10, y - 4), **self.dash)
            self.scribbles.add((x, y // self.textstep))
        return x + 10 + len(text) * 9.5, y - 4 # magic. Depends on the font.

    def fomtext(self, fom, text, time):
        return self.text(text, insert = (self.getlane(fom, 0),
                                         self.getpos(time)))

    def prepare(self, time):
        if self.start == None:
            self.start = time
        self.lastreport = self.start
        duration = datetime.timedelta(microseconds = self.usec)
        self.end = self.start + duration
        delta = datetime.timedelta(milliseconds = self.step)
        n = 0
        while n*delta <= duration:
            t = self.start + n * delta
            y = self.getpos(t)
            label = t.strftime(self.timeformat)
            self.line((0, y), (self.width, y), stroke = self.axis,
                      stroke_width = 1, stroke_dasharray = "20,10,5,5,5,10")
            self.text(label, insert = (0, y - 10), force = True)
            n = n + 1
        self.prep = True

    def ioadd(self, time, fid, seconds):
        duration = datetime.timedelta(microseconds = float(seconds) * 1000000)
        start = time - duration
        y0 = self.getpos(start)
        y1 = self.getpos(time)
        l0 = self.text("L " + fid, insert = (self.iostart, y0))
        l1 = self.text("E " + fid, insert = (self.iostart, y1))
        slot = next((i for i in range(len(self.iolast)) if
                     self.iolast[i] < start), None)
        if slot != None:
            x = self.iolane0 + self.iolane * slot
            self.rect(insert = (x, y0), size = (self.iolane * 3/4, y1 - y0),
                      fill = self.getcolour(str(slot) + str(start)))
            self.iolast[slot] = time
            self.tline(l0, (x, y0), **self.dash)
            self.tline(l1, (x, y1), **self.dash)
        elif not self.warnedio:
            self.warnedio = True
            print "Too many concurrent IO-s. Increase iomax."

    def netbufadd(self, time, buf, qtype, seconds, stime, status, length):
        qname = [
            "msg-recv",
            "msg-send",
            "p-bulk-recv",
            "p-bulk-send",
            "a-bulk-recv",
            "a-bulk-send",
        ]
        if qtype == 0:
            return # skip receives
        start = parsetime(stime)
        duration = datetime.timedelta(microseconds = float(seconds) * 1000000)
        dequeue  = start + duration
        assert start <= dequeue and dequeue <= time
        y0 = self.getpos(start)
        y1 = self.getpos(dequeue)
        y2 = self.getpos(time)
        l0 = self.text("Q " + buf + " " + qname[qtype] + " " + str(length),
                       insert = (self.netstart, y0))
        l2 = self.text("C " + buf, insert = (self.netstart, y2))
        slot = next((i for i in range(len(self.netlast)) if
                     self.netlast[i] < start), None)
        if slot != None:
            x = self.netlane0 + self.netlane * slot
            self.rect(insert = (x, y0), size = (self.netlane * 3/4, y1 - y0),
                      fill = self.getcolour(qname[qtype]))
            self.rect(insert = (x, y1), size = (self.netlane * 1/4, y2 - y1),
                      fill = self.getcolour("cb"))
            self.netlast[slot] = time
            self.tline(l0, (x, y0), **self.dash)
            self.tline(l2, (x, y2), **self.dash)
        elif not self.warnednet:
            self.warnednet = True
            print "Too many concurrent netbufs. Increase netmax."

    def mutex(self, mname, label, time, seconds, addr):
        duration = datetime.timedelta(microseconds = float(seconds) * 1000000)
        start = time - duration
        y0 = self.getpos(start)
        y1 = self.getpos(time)
        exists = addr in self.locks
        if not exists:
            if len(self.locks) >= self.lockmax:
                if not self.warnedlock:
                    self.warnedlock = True                
                    print "Too many locks. Increase lockmax."
                return
            self.locks[addr] = len(self.locks)
        lane = self.locks[addr]
        x = self.locklane0 + self.locklane * lane
        if not exists:
            ly = max(y0, 40)
            self.tline((x, 0), (x, self.height), **self.dash)
            l = self.text(mname + " " + str(addr), insert = (self.lockstart, ly))
            self.tline(l, (x, ly), **self.dash)
        self.rect(insert = (x, y0), size = (self.locklane * 3/4, y1 - y0),
                  fill = self.getcolour(label), stroke = self.axis)

class locality(object):
    def __init__(self, trace, idx):
        self.trace  = trace
        self.foms   = {}
        self.idx    = idx

    def fomadd(self, fom):
        trace = self.trace
        j = len(self.foms)
        for i in range(len(self.foms)):
            if i not in self.foms:
                j = i
                break
        if j > trace.maxfom:
            if trace.warnedfom < j:
                print ("{}: too many concurrent foms, "
                       "increase maxfom to {}".format(fom.time, j))
                trace.warnedfom = j
        self.foms[j] = fom
        fom.loc_idx = j
        fom.loc = self

    def fomdel(self, fom):
        assert self.foms[fom.loc_idx] == fom
        del self.foms[fom.loc_idx]
        
def keep(word):
    return word in tags

def parsetime(stamp):
        #
        # strptime() is expensive. Hard-code.
        # # cut to microsecond precision
        # datetime.datetime.strptime(stamp[0:-3], trace.timeformat)
        #
        # 2016-03-24-09:18:46.359427942
        # 01234567890123456789012345678
        return datetime.datetime(year        = int(stamp[ 0: 4]),
                                 month       = int(stamp[ 5: 7]),
                                 day         = int(stamp[ 8:10]),
                                 hour        = int(stamp[11:13]),
                                 minute      = int(stamp[14:16]),
                                 second      = int(stamp[17:19]),
                                 microsecond = int(stamp[20:26]))
    
def parse(trace, words):
    stamp = words[0]
    tag   = words[1]
    if tag in tags:
        obj        = tags[tag]()
        obj.ctx    = {}
        obj.time   = parsetime(stamp)
        obj.params = words[2:]
        obj.trace  = trace
        if not trace.prep:
            trace.prepare(obj.time)
    else:
        obj = None
    return obj

class record(object):
    def add(self, words):
        key = words[0]
        val = words[1:]
        assert key not in self.ctx
        self.ctx[key] = val
        self.trace = None

    def done(self, trace):
        self.trace = trace
        trace.processed = trace.processed + 1
        if (trace.verb > 0 and
            self.time - trace.lastreport > datetime.timedelta(seconds = 1)):
            print self.time, trace.processed - trace.reported, trace.processed
            trace.lastreport = self.time
            trace.reported   = trace.processed

    def get(self, label):
        return self.ctx[label][0]

    def fomexists(self):
        return "fom" in self.ctx

    def __str__(self):
        return str(self.time)

    def getloc(self):
        loc = int(self.get("locality"))
        if self.trace.loc_nr == 1:
            loc = 0
        assert 0 <= loc and loc < self.trace.loc_nr
        return loc

class fstate(record):
    def done(self, trace):
        state = self.params[2]
        super(fstate, self).done(trace)
        if self.fomexists():
            fom = trace.fomfind(self)
            trace.rect(fill = trace.statecolour(fom),
                       **trace.fomrect(fom, 3, fom.state_time, self.time))
            fom.state_time = self.time
            fom.state = state
            if state == "Finished":
                start = trace.getpos(fom.time)
                end   = trace.getpos(self.time)
                lane  = trace.getlane(fom, 0) - 5
                trace.line((lane, start), (lane, end), stroke = trace.axis,
                           stroke_width = 3)
                self.trace.fomdel(fom)
                del trace.foms[self.get("fom")]
            

class fphase(record):
    def done(self, trace):
        super(fphase, self).done(trace)
        if (len(self.params) in (2, 3) and self.fomexists()):
            fom = trace.fomfind(self)
            trace.rect(fill = trace.fomcolour(fom),
                       **trace.fomrect(fom, 2, fom.phase_time, self.time))
            l = trace.fomtext(fom, fom.phase, fom.phase_time)
            x = trace.getlane(fom, 1)
            if l[0] < x:
                trace.tline(l, (x, l[1]), **trace.dash)
            trace.tline((x, l[1]),
                        (trace.getlane(fom, 2), trace.getpos(fom.phase_time)),
                        **trace.dash)
            fom.phase_time = self.time
            fom.phase = self.params[-1]
            
class fom(record):
    def done(self, trace):
        addr = self.get("fom")
        assert "locality" in self.ctx
        # assert addr not in trace.foms
        trace.foms[addr] = self
        super(fom, self).done(trace)
        self.loc_idx = -1
        self.trace.fomadd(self)
        self.state = "Ready"
        self.phase = "init"
        self.state_time = self.time
        self.phase_time = self.time
        trace.fomtext(self, self.params[5] + str(addr) +
                      "[" + self.get("locality") + "]", self.time)

    def __str__(self):
        return str(self.time) + " " + self.get("fom")

class forq(record):
    def done(self, trace):
        if "locality" not in self.ctx:
            return # ast in 0-locality
        super(forq, self).done(trace)
        loc_id = self.getloc()
        nanoseconds = float(self.params[0][:-1]) # Cut final comma.
        duration = datetime.timedelta(microseconds = nanoseconds / 1000)
        x = self.trace.getloc(loc_id) + 10
        y = self.trace.getpos(self.time - duration)
        trace.tline((x, y), (x, self.trace.getpos(self.time)),
                    stroke = svgwrite.rgb(80, 10, 10, '%'), stroke_width = 5)
        trace.text(self.params[1], connect = True, insert = (x + 10, y))

class ioend(record):
    def done(self, trace):
        super(ioend, self).done(trace)
        trace.ioadd(self.time, self.params[0][:-1], self.params[2][:-1])

class netbuf(record):
    def done(self, trace):
        super(netbuf, self).done(trace)
        assert (self.params[0] == "buf:" and self.params[2] == "qtype:" and
                self.params[4] == "time:" and self.params[6] == "duration:" 
                and self.params[8] == "status:" and self.params[10] == "len:")
        trace.netbufadd(self.time,
                        buf     = self.params[1][:-1],
                        qtype   = int(self.params[3][:-1]),
                        stime   = self.params[5][:-1],
                        seconds = float(self.params[7][:-1]),
                        status  = int(self.params[9][:-1]),
                        length  = int(self.params[11])) # no comma: last one

class mutex(record):
    def setname(self, mname, label):
        self.mname = mname
        self.label = label

    def done(self, trace):
        super(mutex, self).done(trace)
        trace.mutex(self.mname, self.label, self.time,
                    float(self.params[0][:-1]), self.params[1])

class rpcmachwait(mutex):
    def done(self, trace):
        self.setname("rpc-mach", "wait")
        super(rpcmachwait, self).done(trace)

class rpcmachhold(mutex):
    def done(self, trace):
        self.setname("rpc-mach", "hold")
        super(rpcmachhold, self).done(trace)

tags = {
    "fom-descr"         : fom,
    "fom-state"         : fstate,
    "fom-phase"         : fphase,
    "loc-forq-duration" : forq,
    "stob-io-end"       : ioend,
    "net-buf"           : netbuf,
    "rpc-mach-wait"     : rpcmachwait,
    "rpc-mach-hold"     : rpcmachhold
}
