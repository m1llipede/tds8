# pip install python-osc
from pythonosc.udp_client import SimpleUDPClient
c = SimpleUDPClient("192.168.1.50", 8000)
c.send_message("/ping", 9000)  # optional reply-port arg
print("sent")