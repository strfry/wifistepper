import goodrobotics as gr

s = gr.WifiStepper('wsx100.local')
s.connect()


s.run(True, 100)
s.waitms(5000)
s.goto(-1000)
s.waitbusy()
s.stop()
