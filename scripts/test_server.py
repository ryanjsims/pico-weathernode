import socketio
import eventlet
import eventlet.wsgi

sio = socketio.Server()

@sio.event
def weather_event(sid, *data):
    print(f"From {sid}: {data}")

if __name__ == "__main__":
    app = socketio.WSGIApp(sio)
    eventlet.wsgi.server(eventlet.listen(('', 8000)), app)
