#!/usr/bin/env python


import threading
import time
from ws4py.client.threadedclient import WebSocketClient # pip install ws4py
import requests
import json

WS = None # websocket
EXIT = False

headers = ['','','',"'temperature'","'pressure'","'humidity'","'windSpeed'","'windDirection'","'radiation'"]

class HabDecClient(WebSocketClient):
	def received_message(self, m):
		print(m.data)
		dataSend(m.data)
	def opened(self):
		print("Opened Connection")
	def closed(self, code, reason=None):
		print("Closed down", code, reason)

def dataSend(sentence):
	if "sentence" in str(sentence):
		a, b, c, d, temp, press, hum, WD, WS, rad, DiffV, CtrlV = str(str(sentence).split("*")[0]).split(",")
		data = {'temperature':   temp,
                        'pressure':      press,
                        'humidity':      hum,
                        'windSpeed':     WS,
                        'windDirection': WD,
                        'radiation':     rad}
		
		print (data)
		r = requests.put('http://knotkovi.tplinkdns.com:3000/data', json = data )

def RequestRttyStream():
	global WS
	while WS and not EXIT:
		WS.send("cmd::liveprint")
		WS.send("cmd::sentence")
		# WS.send("*") # or just send anything to be notified with cmd::info:sentence
		time.sleep(.5)


def main(i_srv = 'ws://127.0.0.1:5555'):
	global WS
	WS = HabDecClient( i_srv )
	WS.connect()

	threading.Thread( target = lambda: WS.run_forever() ).start()

	# you don't need to explicitly poll for data - server will push it to you
	# threading.Thread( target = RequestRttyStream ).start()

	# exit on Ctrl-C
	while 1:
		try:
			time.sleep(1)
		except KeyboardInterrupt:
			global EXIT
			EXIT = True
			WS.close()
			return


if __name__ == '__main__':
	main()
