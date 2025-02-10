import subprocess


def run_command(command: str) -> tuple[str, str]:
    try:
        result = subprocess.run(
            command,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            check=True,
        )
        return result.stdout, result.stderr
    except subprocess.CalledProcessError as e:
        raise RuntimeError(e.stderr)


def check_wmctrl_installation():
    try:
        subprocess.check_output(["wmctrl", "-m"], stderr=subprocess.STDOUT, text=True)
    except subprocess.CalledProcessError:
        print(
            "wmctrl is not installed on your system. "
            "You can install it on Debian/Ubuntu using the following command: \n\tsudo apt-get install wmctrl"
        )
        exit(1)


def exit_with_error(msg: str, code: int = 1):
    print(msg)
    exit(code)
