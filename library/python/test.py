import goodrobotics as gr

s = gr.WifiStepper(host='wsx200.local')
s.connect()


s.run(True, 100)
s.waitms(5000)
s.goto(-1000)
s.waitbusy()
s.stop()


import goodrobotics as gr
s = gr.WifiStepper(proto=gr.ComCrypto, host='wsx100.local', key="newpass1")
s.connect()


s.run(True, 100, queue=2)
s.waitms(5000, queue=2)
s.goto(-1000, queue=2)
s.waitbusy(queue=2)
s.stop(queue=2)
s.waitms(1000, queue=2)
s.runqueue(2, queue=2)
