require 'httparty'
require 'nokogiri'

class ApplicationController < ActionController::Base
  before_filter :is_session_valid

  # Prevent CSRF attacks by raising an exception.
  # For APIs, you may want to use :null_session instead.
  protect_from_forgery with: :exception

  protected

  def is_session_valid
    if session[:userid]
       # set current user object to @current_user object variable
      Rails.logger.info("Existing session with Cookie = #{request.headers['HTTP_COOKIE']}...")
      return true
    else
      flash[:notice] = "Invalid Session. Please login via signin.seagate.com"
      flash[:color]= "Forbidden"
      render(:file => File.join(Rails.root, 'public/403.html'), :status => 403, :layout => false)
      return false
    end
  end

  def authenticate_user
    if session[:userid]
       # set current user object to @current_user object variable
      Rails.logger.info("Already Logged in user...")
      return true
    else
      # Validate the session agains Auth server.
      # request.headers.each { |key, value|
      #   Rails.logger.info("key = #{key} and value = #{value}")
      # }
      Rails.logger.info("New Session with tokens = #{@auth_tokens}")

      begin
        auth_response = HTTParty.get(Rails.application.config.s3_auth_api, :ssl_ca_file => Rails.application.config.s3_auth_api_cert, query: @auth_tokens|| {})
      rescue => exception
        Rails.logger.info(exception.backtrace)
        render(:file => File.join(Rails.root, 'public/500.html'), :status => 500, :layout => false)
        return false
      end
      # auth_response = HTTParty.get('https://s3web.local:9443/session/saml', :verify => false)
      if auth_response.code == 200
        xml_doc = Nokogiri::XML(auth_response.body)
        session[:userid] = xml_doc.at_css("SessionTokenResponse SessionTokenResult User UserId").text
        session[:username] = xml_doc.at_css("SessionTokenResponse SessionTokenResult User UserName").text
        session[:accountname] = xml_doc.at_css("SessionTokenResponse SessionTokenResult User AccountName").text
        session[:accountid] = xml_doc.at_css("SessionTokenResponse SessionTokenResult User AccountId").text

        session[:access_key] = xml_doc.at_css("SessionTokenResponse SessionTokenResult Credentials AccessKeyId").text
        session[:secret_key] = xml_doc.at_css("SessionTokenResponse SessionTokenResult Credentials SecretAccessKey").text
        session[:access_token] = xml_doc.at_css("SessionTokenResponse SessionTokenResult Credentials SessionToken").text
        return true
      else
        flash[:notice] = "Invalid Session. Please login via signin.seagate.com"
        flash[:color]= "Forbidden"
        render(:file => File.join(Rails.root, 'public/403.html'), :status => 403, :layout => false)
      end
      return false
    end
  end

end
