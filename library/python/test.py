import time

import goodrobotics as gr

s = gr.WifiStepper('wsx100.local')
s.connect()

time.sleep(1000)
