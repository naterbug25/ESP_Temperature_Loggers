import requests
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from datetime import datetime, timedelta
import time
def Send_Email (_Temperature_S0,_Temperature_S1):
    sender_email = "nathanhuber2014@yahoo.com"  # Sending email account
    password = "qcrwmmmjspwlmobb"  # Sender password
    recipient_email = "4193058328@tmomail.net"

    # Create the MIME object
    message = MIMEMultipart()
    message['From'] = sender_email
    message['To'] = recipient_email
    message['Subject'] = 'Alert - Aquarium Temperature'

    # Email body
    body = "Sensor 0: " + str(_Temperature_S0) + "\nSensor 1: " + str(_Temperature_S1)
    message.attach(MIMEText(body, 'plain'))

    # Connect to Yahoo's SMTP server
    smtp_server = 'smtp.mail.yahoo.com'
    smtp_port = 587
    server = smtplib.SMTP(smtp_server, smtp_port)
    server.starttls()

    # Login to your Yahoo account
    server.login(sender_email, password)

    # Send the email
    server.sendmail(sender_email, recipient_email, message.as_string())

    # Quit the server
    server.quit()
    print("Message sent!")


def Get_Temperature(_url):
    try:
        _response = requests.get(_url) # Get the text from ip
        _Temperature = _response.text # Store as string
        _Temperature_S0,_Temperature_S1 = _Temperature.splitlines() # Split the new lines
    except Exception as e:
        _Temperature_S0 = 0
        _Temperature_S1 = 0
        print(e)
    print("Sensor 0:", _Temperature_S0)
    print("Sensor 1:", _Temperature_S1)
    return _Temperature_S0,_Temperature_S1


if __name__ == "__main__":
    Time_Message_Sent = datetime(2024, 1, 1, 0, 0, 0)
    Max_Temp = 82
    Min_Temp = 74
    while (True):
        Temperature_S0,Temperature_S1 = Get_Temperature('http://192.168.1.16')
        if Temperature_S0 != 0 and Temperature_S1 != 0:
            if ((float(Temperature_S0) < Min_Temp) or (float(Temperature_S0) > Max_Temp)) or ((float(Temperature_S1) < Min_Temp) or (float(Temperature_S1) > Max_Temp)):
                Time_Delta = datetime.now() - Time_Message_Sent # Calculate dif in time
                if Time_Delta >= timedelta(minutes=60): # Only allow message to be sent 
                    Send_Email (Temperature_S0,Temperature_S1) # Send email
                    Time_Message_Sent = datetime.now() # Update time last sent
        time.sleep(10)
        # # Stop at mid night
        # if (datetime.now().hour == 0) and (datetime.now().minute == 0):
        #     break
            

