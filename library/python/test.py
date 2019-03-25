import goodrobotics as gr

s = gr.WifiStepper('10.1.10.247')
s.connect()


s.run(True, 1000, target=1)
s.waitms(5000, target=1)
s.goto(-1000, target=1)
s.stop(target=1)
