import json
from flask import Flask, render_template
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt

app = Flask(__name__)
app.config['SECRET_KEY'] = 'greenhouse_secret!'
socketio = SocketIO(app, cors_allowed_origins="*")

MQTT_BROKER = "broker.hivemq.com"
TOPIC_DATA = "greenhouse/data"       
TOPIC_CONTROL = "greenhouse/control" 

def on_connect(client, userdata, flags, rc):
    print("Mạng: Đã kết nối MQTT Broker thành công!")
    client.subscribe(TOPIC_DATA)

def on_message(client, userdata, msg):
    if msg.topic == TOPIC_DATA:
        try:
            payload = msg.payload.decode()
            data = json.loads(payload)
            socketio.emit('sensorData', data)
        except Exception as e:
            print("Lỗi giải mã JSON từ ESP32:", e)

mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('controlDevice')
def handle_device_control(json_data):
    print("Lệnh điều khiển từ Web:", json_data)
    mqtt_client.publish(TOPIC_CONTROL, json.dumps(json_data))

if __name__ == '__main__':
    mqtt_client.connect(MQTT_BROKER, 1883, 60)
    mqtt_client.loop_start()
    
    print("=== WEB SERVER PYTHON ĐANG CHẠY ===")
    socketio.run(app, host='0.0.0.0', port=5000, debug=True)