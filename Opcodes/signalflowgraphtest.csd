<CsoundSynthesizer>
<CsOptions>
csound -RWfo signalflowgraphtest.wav
</CsOptions>
<CsInstruments>

sr = 44100
ksmps = 128
nchnls = 2
0dbfs = 1

alwayson "baz"

instr 1
ifno  ftgentmp   0, 0, 512, 10, 1
print ifno
asignal poscil3 .25, 440, ifno
adummy = 0
outs adummy, asignal
endin

instr 2
print ftlen(p4)
endin

instr 3
ifno  ftgenonce  0, 0, 512, 10, 1, p4, p5
print ftlen(ifno)
print ifno, p4
asignal poscil3 .25, 440, ifno
adummy = 0
outs asignal, adummy
endin

instr baz
ifno  ftgenonce  0, 0, 512, 10, 4, 3, 2, 1
print ftlen(ifno)
print ifno, p4
asignal poscil3 .25, 60, ifno
outs asignal, asignal
endin


</CsInstruments>
<CsScore>
i 1 0 10
i 2 2 1 101
i 1 5 10
i 2 7 1 102
i 2 12 1 101 
i 2 17 1 102 

i 3 20 10 1 1
i 3 21 10 2 1
i 3 22 10 1 2
i 3 23 10 1 1
e
</CsScore>
</CsoundSynthesizer>

