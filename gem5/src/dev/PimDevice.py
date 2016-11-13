
from m5.params import *
from Device import BasicPioDevice


class PimDevice(BasicPioDevice):
    type = 'PimDevice'
    cxx_header = "dev/pimDevice.hh"
    devicename = Param.String(" Hellooooooooooo! :D ")

