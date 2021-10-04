#!/usr/bin/env python

# DRP Demo Endpoint

import asyncio
import websockets
import json

class DRP_Endpoint:

    def __init__(self, wsConn, drpNode, endpointID, endpointType):
        self.wsConn = wsConn
        self.drpNode = drpNode
        if (self.wsConn):
            self.wsConn.drpEndpoint = self
        self.EndpointID = endpointID
        self.EndpointType = endpointType
        self.EndpointCmds = {}

        self.ReplyHandlerQueue = {}
        self.StreamHandlerQueue = {}
        self.TokenNum = 1

        self.Subscriptions = {}

        self.openCallback = None

        self.closeCallback = None
        self.RegisterMethod("getCmds", "GetCmds")

        asyncio.ensure_future(self.wsReceiveLoop())

    async def wsReceiveLoop(self):
        while True:
            #print(f"> Waiting for next incoming message...")
            json_in = await self.wsConn.recv()
            print(f"< {json_in}")
            self.ReceiveMessage(json_in)

    def GetToken(self):
        token = self.TokenNum
        self.TokenNum += 1
        return token

    def AddReplyHandler(self, callback):
        token = self.GetToken()
        self.ReplyHandlerQueue[token] = callback;
        return token

    def DeleteReplyHandler(self, token):
        del self.ReplyHandlerQueue[token]

    def AddStreamHandler(self, callback):
        streamToken = self.GetToken()
        self.StreamHandlerQueue[streamToken] = callback
        return streamToken

    def DeleteStreamHandler(self, streamToken):
        del self.StreamHandlerQueue[streamToken]

    def RegisterMethod(self, methodName, method):
        thisEndpoint = self
        if (callable(method)):
            thisEndpoint.EndpointCmds[methodName] = method
        elif (callable(getattr(self, method, None))):
            thisEndpoint.EndpointCmds[methodName] = getattr(self, method, None)
        else:
            typeObj = getattr(self, method, None)
            typeName = ""
            if typeObj is None:
                typeName = "None"
            else:
                typeName = typeObj.__name__
            thisEndpoint.log(f"Cannot add EndpointCmds[{cmd}] -> sourceObj[{method}] of type {typeName}")

    async def SendCmd(self, serviceName, method, params, awaitResponse, routeOptions):
        thisEndpoint = self
        returnVal = None
        token = None

        async def awaitFunc(returnData):
            return returnData

        if (awaitResponse):
            # TODO - Update this to add a go style channel to the ReplyHanderQueue
            replyQueue = asyncio.Queue()
            token = thisEndpoint.AddReplyHandler(replyQueue);
        else:
            # We don't expect a response; leave reply token null
            token = None

        # Send command
        sendCmd = DRP_Cmd(serviceName, method, params, token, routeOptions)
        print(f"> {json.dumps(sendCmd)}")
        await thisEndpoint.wsConn.send(json.dumps(sendCmd))
        #print(f"Command sent")

        if (awaitResponse):
            # Get data from queue
            # Get data from queue
            returnVal = await replyQueue.get()
        else:
           returnVal = None

        return returnVal

    async def ProcessCmd(self, message):
        thisEndpoint = self
        cmdResults = {
            "status": 0,
            "output": None
        }

        if ("routeOptions" not in message or message.routeOptions.tgtNodeID == thisEndpoint.drpNode.NodeID):
            # Execute locally

            # Is the message meant for the default DRP service?
            if ("serviceName" not in message or message["serviceName"] == "DRP"):
                # Yes - execute here
                if ("method" not in message):
                    cmdResults["output"] = "message.method not specified"
                elif (message["method"] not in thisEndpoint.EndpointCmds):
                    cmdResults["output"] = f"{message['method']} not in EndpointCmds"
                elif (not callable(thisEndpoint.EndpointCmds[message["method"]])):
                    cmdResults["output"] = f"EndpointCmds[{message['method']}] is not callable"
                else:
                    # Execute method
                    try:
                        cmdResults["output"] = await thisEndpoint.EndpointCmds[message.get("method",None)](message.get("params",None), thisEndpoint, message.get("token",None))
                        cmdResults["status"] = 1
                    except ():
                        cmdResults["output"] = "Could not execute"
            else:
                # No - treat as ServiceCommand
                try:
                    cmdResults["output"] = await thisEndpoint.drpNode.ServiceCommand(message, thisEndpoint)
                    cmdResults["status"] = 1
                except (err):
                    cmdResults["output"] = err.message;

        else:
            # This message is meant for a remote node

            try:
                targetNodeEndpoint = await thisEndpoint.drpNode.VerifyNodeConnection(message.routeOptions.tgtNodeID);
                cmdResults["output"] = await targetNodeEndpoint.SendCmd(message.serviceName, message.method, message.params, true, null);
            except:
                # Either could not get connection to node or command send attempt errored out
                x = 1
        # Reply with results
        if ("token" in message and message["token"] is not None):
            await thisEndpoint.SendReply(message["token"], cmdResults["status"], cmdResults["output"])

    # SendReply
    async def SendReply(self, token, status, payload):
        if (self.wsConn.state == 1):
            replyString = None
            try:
                replyString = json.dumps(DRP_Reply(token, status, payload, None, None))
            except:
                replyString = json.dumps(DRP_Reply(token, 0, "Failed to stringify response", None, None))
            print(f"> {replyString}")
            await self.wsConn.send(replyString);
            return 0
        else:
            return 1

    async def ProcessReply(self, message):
        thisEndpoint = self

        # Yes - do we have the token?
        if (message["token"] in thisEndpoint.ReplyHandlerQueue):

            # We have the token - execute the reply callback
            thisEndpoint.ReplyHandlerQueue[message["token"]].put_nowait(message)

            # Delete if we don't expect any more data
            if message["status"] < 2:
                del thisEndpoint.ReplyHandlerQueue[message["token"]]
            return False;
        else:
            # We do not have the token - tell the sender we do not honor this token
            return True;

    def ReceiveMessage(self, rawMessage):
        thisEndpoint = self
        message = {}
        try:
            message = json.loads(rawMessage);
        except:
            thisEndpoint.log("Received non-JSON message, disconnecting client... %s", wsConn._socket.remoteAddress);
            thisEndpoint.wsConn.close();
            return

        if ("type" not in message):
            thisEndpoint.log("No message.type; here's the JSON data..." + rawMessage);
            return

        def default():
            thisEndpoint.log("Invalid message.type; here's the JSON data..." + rawMessage);

        switcher = {
            "cmd": thisEndpoint.ProcessCmd,
            "reply": thisEndpoint.ProcessReply,
            #"stream": await thisEndpoint.ProcessStream
        }

        func = switcher.get(message["type"], default);
        asyncio.ensure_future(func(message))

    # WatchStream

    # RegisterSubscription

    async def GetCmds(self, params, endpoint, token):
        return list(self.EndpointCmds.keys())

    def log(self, logMessage):
        thisEndpoint = self
        if (thisEndpoint.drpNode is not None):
            thisEndpoint.drpNode.log(logMessage)
        else:
            print(logMessage)

class DRP_Cmd(dict):
    def __init__(self, serviceName, method, params, token, routeOptions):
        dict.__init__(self, type="cmd", serviceName=serviceName, method=method, params=params, token=token, routeOptions=routeOptions)

class DRP_Reply(dict):
    def __init__(self, token, status, payload, srcNodeID, tgtNodeID):
        dict.__init__(self, type="reply", token=token, status=status, payload=payload, srcNodeID=srcNodeID, tgtNodeID=tgtNodeID)

class DRP_Stream:
    def __init__(self, token, status, payload, srcNodeID, tgtNodeID):
        dict.__init__(self, type="stream", token=token, status=status, payload=payload, srcNodeID=srcNodeID, tgtNodeID=tgtNodeID)

class DRP_RouteOptions:
    def __init__(srcNodeID, tgtNodeID, routePath):
        self.srcNodeID = srcNodeID;
        self.tgtNodeID = tgtNodeID;
        self.routeHistory = routeHistory

async def hello(websocket, path):
    while True:
        json_in = await websocket.recv()
        print(f"< {json_in}")

        json_dict = json.loads(json_in)
        if "type" in json_dict:
            # Get the type
            msg_type = json_dict["type"]
            print(f"> This is a '{msg_type}' message")
        else:
            # No type
            print(f"> Unknown message -> '{json_in}'")

        greeting = '{"reply":"bleh"}'

        await websocket.send(greeting)
        print(f"> {greeting}")

async def wsRecv(websocket, path):
    myEndpoint = DRP_Endpoint(websocket, None, "1234567890", "Server")
    while True:
        json_in = await websocket.recv()
        myEndpoint.ReceiveMessage(json_in)

#start_server = websockets.serve(wsRecv, "localhost", 8765)
#print("Started WS listener")
#asyncio.get_event_loop().run_until_complete(start_server)
#asyncio.get_event_loop().run_forever()

# DRP Test Client
async def drpTestClient():
    uri = "ws://localhost:8080"
    print(f"Connecting to -> {uri}")
    async with websockets.connect(uri) as websocket:

        print(f"Connected!")
        myEndpoint = DRP_Endpoint(websocket, None, "1234567890", "Client")

        print(f"Sending hello...")
        returnData = await myEndpoint.SendCmd("DRP", "hello", { "userAgent": "python", "user": "someuser", "pass": "somepass" }, True, None)

        print(f"Sending getCmds...")
        returnData = await myEndpoint.SendCmd("DRP", "getCmds", None, True, None)

        # Using this to keep the client alive; need to figure out a cleaner way
        dummyQueue = asyncio.Queue()
        await dummyQueue.get()

asyncio.run(drpTestClient())
