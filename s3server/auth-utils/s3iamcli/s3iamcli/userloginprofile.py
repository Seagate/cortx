from s3iamcli.cli_response import CLIResponse

class UserLoginProfile:
    def __init__(self, iam_client, cli_args):
        self.iam_client = iam_client
        self.cli_args = cli_args

    def create(self):
        if(self.cli_args.name is None):
            message = "User name is required for user login-profile creation"
            CLIResponse.send_error_out(message)

        if(self.cli_args.password is None):
            message = "User password is required for user login-profile creation"
            CLIResponse.send_error_out(message)
        user_args = {}
        user_args['UserName'] = self.cli_args.name
        user_args['Password'] = self.cli_args.password
        user_args['PasswordResetRequired'] = False
        if(self.cli_args.password_reset_required):
            user_args['PasswordResetRequired'] = True
        try:
            result = self.iam_client.create_login_profile(**user_args)
        except Exception as ex:
            message = "Failed to create userloginprofile.\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

        profile = (result['LoginProfile'])
        print("Login Profile %s %s %s" % (profile['CreateDate'], profile['PasswordResetRequired'], profile['UserName']))

    def get(self):
        if(self.cli_args.name is None):
            message = "User name is required for getting Login Profile"
            CLIResponse.send_error_out(message)

        user_args = {}
        user_args['UserName'] = self.cli_args.name
        try:
            result = self.iam_client.get_login_profile(**user_args)
        except Exception as ex:
            message = "Failed to get Login Profile for "+ user_args['UserName'] + "\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

        profile = (result['LoginProfile'])
        print("Login Profile %s %s %s" % (profile['CreateDate'], profile['PasswordResetRequired'], profile['UserName']))

    def update(self):
        if(self.cli_args.name is None):
            message = "UserName is required for UpdateUserLoginProfile"
            CLIResponse.send_error_out(message)
        user_args = {}
        user_args['UserName'] = self.cli_args.name
        if(not self.cli_args.password is None):
            user_args['Password'] = self.cli_args.password
        user_args['PasswordResetRequired'] = False
        if(self.cli_args.password_reset_required):
            user_args['PasswordResetRequired'] = True
        if(self.cli_args.password is None) and (self.cli_args.password_reset_required is False) and (self.cli_args.no_password_reset_required is False):
            message = "Please provide password or password-reset flag"
            CLIResponse.send_error_out(message)
        try:
            result = self.iam_client.update_login_profile(**user_args)
            message = "UpdateUserLoginProfile is successful"
            CLIResponse.send_success_out(message)
        except Exception as ex:
            message = "UpdateUserLoginProfile failed\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

    def changepassword(self):
        if(self.cli_args.old_password is None):
            message = "OldPassword is required for changing user password"
            CLIResponse.send_error_out(message)
        if(self.cli_args.new_password is None):
            message = "NewPassword is required for changing user password"
            CLIResponse.send_error_out(message)

        user_args = {}
        user_args['OldPassword'] = self.cli_args.old_password
        user_args['NewPassword'] = self.cli_args.new_password
        try:
            result = self.iam_client.change_password(**user_args)
            message = "ChangePassword is successful"
            CLIResponse.send_success_out(message)
        except Exception as ex:
            message = "ChangePassword failed\n"
            message += str(ex)
            CLIResponse.send_error_out(message)
