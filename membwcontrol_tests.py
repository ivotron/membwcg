import subprocess


def test_module_loads_ok():
    cmd = "modprobe membwcontrol.ko"
    subprocess.check_output(cmd.split())

    cmd = "rmmod membwcontrol"
    subprocess.check_output(cmd.split())
