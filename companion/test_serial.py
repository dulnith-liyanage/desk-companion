import serial
import time
ser = serial.Serial()
ser.port = '/dev/cu.usbmodem2101'
ser.baudrate = 115200
ser.dtr = True
ser.rts = False
ser.open()
time.sleep(2.0)
ser.write(b'M')
ser.flush()
print("Sent 'M' with DTR=True")
ser.close()
