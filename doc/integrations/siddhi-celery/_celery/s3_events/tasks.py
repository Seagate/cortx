import app
@app.app.task
def event_test(x, y):
    return x * y + 15

