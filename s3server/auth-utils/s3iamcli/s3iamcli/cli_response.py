import sys

# Ths Class has static methods to handle CLI exist messages and status

class CLIResponse():

    @staticmethod
    def send_error_out(message, code=1):
        print(message)
        sys.exit(code)

    @staticmethod
    def send_success_out(message, code=0):
        print(message)
        sys.exit(code)


