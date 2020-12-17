def run():
    siddhiManager = SiddhiManager()
    # Siddhi Query to filter events with volume less than 150 as output
    siddhiApp = "define stream cseEventStream (symbol string, price float, volume long);" + \
                "@info(name = 'query1') " + \
                "from cseEventStream[volume < 150] " + \
                "select symbol, price " + \
                "insert into outputStream;"
    # Generate runtime
    siddhiAppRuntime = siddhiManager.createSiddhiAppRuntime(siddhiApp)
    # Add listener to capture output events
    class QueryCallbackImpl(QueryCallback):
        def receive(self, timestamp, inEvents, outEvents):
            PrintEvent(timestamp, inEvents, outEvents)
    siddhiAppRuntime.addCallback("query1",QueryCallbackImpl())
    # Retrieving input handler to push events into Siddhi
    inputHandler = siddhiAppRuntime.getInputHandler("cseEventStream")
    # Starting event processing
    siddhiAppRuntime.start()
    # Sending events to Siddhi
    inputHandler.send(["IBM",700.0,LongType(100)])
    inputHandler.send(["WSO2", 60.5, LongType(200)])
    inputHandler.send(["GOOG", 50, LongType(30)])
    inputHandler.send(["IBM", 76.6, LongType(400)])
    inputHandler.send(["WSO2", 45.6, LongType(50)])
    # Wait for response
    sleep(0.1)
