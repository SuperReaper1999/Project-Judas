#!/usr/bin/env python3
"""Independent one-dimensional continuum buoyancy+accepted linear drag ODE.
Not an engine replacement, and not proof of full coupled PBF correctness.
"""
import json
# Same half-density 0.6 m box, release centre 0.7 m, measured rotated
# quiescent release surface. No tuned force, threshold or settling window.
surface=1.790144;duration=4.;height=.6;g=9.81;dragRate=2.
def derivative(y,v):
    wet=max(0.,min(1.,(surface-(y-height/2))/height))
    return v,g*(2*wet-1)-dragRate*wet*v
for hz in [60,120,240,480,9600]:
    h=1./hz;y=.7;v=0.
    for _ in range(int(duration*hz)):
        a,b=derivative(y,v);c,d=derivative(y+h*a/2,v+h*b/2)
        e,f=derivative(y+h*c/2,v+h*d/2);j,k=derivative(y+h*e,v+h*f)
        y+=h*(a+2*c+2*e+j)/6;v+=h*(b+2*d+2*f+k)/6
    print(json.dumps(dict(hz=hz,y=y,velocity=v,existingSpeedThreshold=.5,passes=abs(v)<.5)))
