'''
Created on 13 dec. 2014

@author: mvturnho
'''
import serial
import re
from random import randint

# Please note that, 13 = COM14. This is the incoming COM port for my PC (windows 7). Might be different for your machine.
ser = serial.Serial(4, 38400, 8, 'N', 1, timeout=0, writeTimeout=0, interCharTimeout=0);  # open first serial port. Read timeout = timeout = 1sec. writeTimeout = 2 secs
print ser.name   ;  # check which port was really used;

Flag = 0;

a = re.compile("ATE0");
#================================================
# Following functions returns string after '\r' is read
# All communications (queries) from "Torque" ends with '\r' as EOL
#================================================
def readLine():
    str = ""
    while 1:
        ch = ser.read()
        if(ch == '>'):
            continue;
        if(ch == '\r'):
            break;
        str += ch
    print str
    return str
    
A = 0;B = 0;C = 0;D = 0;
    
def rpmtoobd(val):
    val = val * 4;
    A = val / 256;
    B = val % 256;
    retval = format(A, '02x') + " " + format(B, '02x');
    return retval 

def obdRevConvert_04(val):
    A = (255 * val) / 100;
    retval = format(A, '02x');
    return retval;

#================================================
# Following is the main code for communicating with "Torque Lite"
#================================================
# First, receive commands, untill ECHO OFF is received (ATE0)
load = 0;
thr = 0;
speed = 0;
rpm = 878;

Received_str = "";
while 1:
#    print "READ";
    if(load > 100):  load = 0;
    if(thr > 100):  thr = 0;
    if(speed > 255): speed = 0;
    Received_str = readLine();
#    print Received_str;
    if (Received_str == "ATD"):
        ser.write("OK\r")
        ser.write(">")
    elif (Received_str == "ATZ"):
        ser.write("ELM327 v1.5\r")
        ser.write("OK\r")
        ser.write(">")   
    elif (Received_str == "ATL1"):
        ser.write("\rOK\r")
        ser.write(">")
    elif (Received_str == "ATE0"):
        ser.write("\rOK\r")
        ser.write(">")
    elif (Received_str == "ATM0"):
        ser.write("\r\n?\r\n")
        ser.write(">")
    elif (Received_str == "ATL0"):
        ser.write("\rOK\r")
        ser.write(">")
    elif (Received_str == "ATS0"):
        ser.write("\rOK\r")
        ser.write(">")
    elif (Received_str == "ATH0"):
        ser.write("\rOK\r")
        ser.write(">")
    elif (Received_str == "ATS0"):
        ser.write("\rOK\r")
        ser.write(">")
    elif (Received_str == "ATSP0"):
        ser.write("\rOK\r")
        ser.write(">")
    elif (Received_str == "ATRV"):
        ser.write("\r1.2\r")
        ser.write("OK\r")
        ser.write(">")   
    elif (Received_str == "0100"):
        ser.write("\r41 00 98 3B A0 11\r")
        ser.write(">")
    elif (Received_str == "0101"):
        ser.write("\r41 01 02 0E E0 00\r")
        ser.write(">")    
    elif (Received_str == "AT H1"):  # note there is a space between AT and H1
        ser.write("\rOK\r")
        ser.write(">")
    elif (Received_str == "AT H0"):  # note there is a space between AT and H0
        ser.write("\rOK\r")
        ser.write(">")
    elif (Received_str == "0104"):  # Engine load
        print "engine load"
        ser.write("\r41 04 " + obdRevConvert_04(load) + "\r")
        ser.write(">")
        load = load + 1;
    elif (Received_str == "0105"):  # Coolant temperature
        ser.write("\r41 05 80\r")
        ser.write(">")
    elif (Received_str == "010A"):  # Fuel pressure
        print "# Fuel pressure"
        ser.write("\r41 0A 16\r")
        ser.write(">")
    elif (Received_str == "010B"):  # Intake manifold absolute pressure
        ser.write("\r41 0B 66\r")
        ser.write(">")
    elif (Received_str == "010C"):  # Engine RPM
        print "# Engine RPM"
        retval = val = rpmtoobd(rpm);
        print "41 0C " + retval;
        ser.write("\r41 0C " + retval + "\r");  # 0D CA\r")
        ser.write(">")
        rpm = rpm + 1;
    elif (Received_str == "010D"):  # Vehicle speed
        print "# Vehicle speed " + str(speed)
        print "41 0D " + format(speed, '02x')
        ser.write("\r41 0D " + format(speed, '02x') + "\r");
        ser.write(">")
        speed = speed + 1;
    elif (Received_str == "010F"):  # Intake air temperature
        print "# Intake air temperature"
        ser.write("\r41 0F 3B\r")
        ser.write(">")
    elif (Received_str == "0110"):  # MAF Air flow rate
        print "# MAF air flow rate"
        ser.write("\r41 10 D9 C8\r")
        ser.write(">")
    elif (Received_str == "010F"):  # Intake air temperature
        ser.write("\r41 0F D9\r")
        ser.write(">")
    elif (Received_str == "010E"):  # Timing advance
        ser.write("\r41 0E DA\r")
        ser.write(">")
    elif (Received_str == "0111"):  # Throttle position
        print "# Throttle position";
        ser.write("\r41 11 " + obdRevConvert_04(thr) + "\r");
        ser.write(">");
        thr = thr + randint(0,3);
    elif (Received_str == "012F"):  # Fuel level input
            ser.write("\r41 2F DC\r")
            ser.write(">")
    elif (Received_str == "0142"):  # Control module voltage
            ser.write("\r41423646368\r")
            ser.write(">")
    elif (Received_str == "0114"):  # O2 sensor 1
            ser.write("\r4114DE\r")
            ser.write(">")
    elif (Received_str == "0115"):  # O2 sensor 2
            ser.write("\r41 15 DF\r")
            ser.write(">")
    elif (Received_str == "0116"):  # O2 sensor 3
            ser.write("\r41 16 E0\r")
            ser.write(">")
    elif (Received_str == "0117"):  # O2 sensor 4
            ser.write("\r41 17 E1\r")
            ser.write(">")
    elif (Received_str == "0118"):  # O2 sensor 5
            ser.write("\r41 18 E1\r")
            ser.write(">")
    elif (Received_str == "0119"):  # O2 sensor 6
            ser.write("\r41 19 E3\r")
            ser.write(">")
    elif (Received_str == "011A"):  # O2 sensor 7
            ser.write("\r41 1A E4\r")
            ser.write(">")
    elif (Received_str == "011B"):  # O2 sensor 8
            ser.write("\r41 1B E5\r")
            ser.write(">")
    elif (Received_str == "0146"):  # Ambient air temperature
            ser.write("\r41 46 E5\r")
            ser.write(">")
    elif (Received_str == "0133"):  # Barometric pressure
            ser.write("\r41 33 E6\r")
            ser.write(">")
#    print Received_str;
      
    
print "DONE"

ser.close()  # close port
