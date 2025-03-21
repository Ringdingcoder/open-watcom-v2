# wasm Builder Control file
# =========================

set PROJNAME=wasm

set BINTOOL=0

set PROJDIR=<CWD>

[ INCLUDE "<OWROOT>/build/prolog.ctl" ]

[ INCLUDE "<OWROOT>/build/defrule.ctl" ]

[ BLOCK <BLDRULE> rel ]
#======================
    cdsay "<PROJDIR>"

[ BLOCK <BINTOOL> build ]
#========================
    cdsay "<PROJDIR>"
    <CPCMD> <OWOBJDIR>/bwasm.exe "<OWROOT>/build/<OWOBJDIR>/bwasm<CMDEXT>"

[ BLOCK <BINTOOL> clean ]
#========================
    echo rm -f "<OWROOT>/build/<OWOBJDIR>/bwasm<CMDEXT>"
    rm -f "<OWROOT>/build/<OWOBJDIR>/bwasm<CMDEXT>"

[ BLOCK <BLDRULE> rel cprel ]
#============================
    <CCCMD> dos386/wasm.exe     "<OWRELROOT>/binw/"
    <CCCMD> dos386/wasm.sym     "<OWRELROOT>/binw/"
    <CCCMD> dosi86/wasm.exe     "<OWRELROOT>/binw/wasmr.exe"
    <CCCMD> dosi86/wasm.sym     "<OWRELROOT>/binw/wasmr.sym"
    <CCCMD> os2386/wasm.exe     "<OWRELROOT>/binp/"
    <CCCMD> os2386/wasm.sym     "<OWRELROOT>/binp/"
    <CCCMD> nt386/wasm.exe      "<OWRELROOT>/binnt/"
    <CCCMD> nt386/wasm.sym      "<OWRELROOT>/binnt/"
    <CCCMD> linux386/wasm.exe   "<OWRELROOT>/binl/wasm"
    <CCCMD> linux386/wasm.sym   "<OWRELROOT>/binl/"
    <CCCMD> ntaxp/wasm.exe      "<OWRELROOT>/axpnt/"
    <CCCMD> ntaxp/wasm.sym      "<OWRELROOT>/axpnt/"
    <CCCMD> qnx386/wasm.exe     "<OWRELROOT>/qnx/binq/wasm"
    <CCCMD> qnx386/wasm.sym     "<OWRELROOT>/qnx/sym/"
    <CCCMD> rdos386/wasm.exe    "<OWRELROOT>/rdos/"
    <CCCMD> rdos386/wasm.sym    "<OWRELROOT>/rdos/"

    <CCCMD> bsdx64/wasm.exe     "<OWRELROOT>/binb64/wasm"
    <CCCMD> ntx64/wasm.exe      "<OWRELROOT>/binnt64/"
    <CCCMD> linuxx64/wasm.exe   "<OWRELROOT>/binl64/wasm"
    <CCCMD> linuxx64/wasm.sym   "<OWRELROOT>/binl64/"
    <CCCMD> linuxarm/wasm.exe   "<OWRELROOT>/arml/wasm"
    <CCCMD> linuxa64/wasm.exe   "<OWRELROOT>/arml64/wasm"
    <CCCMD> osxx64/wasm.exe     "<OWRELROOT>/bino64/wasm"
    <CCCMD> osxarm/wasm.exe     "<OWRELROOT>/armo/wasm"
    <CCCMD> osxa64/wasm.exe     "<OWRELROOT>/armo64/wasm"

[ BLOCK . . ]

[ INCLUDE "<OWROOT>/build/epilog.ctl" ]
