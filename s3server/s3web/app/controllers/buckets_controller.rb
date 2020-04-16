
require 'mixlib/shellout'

class BucketsController < ApplicationController
  def index  # Display all buckets
    s3cmd = Mixlib::ShellOut.new("s3cmd ls --access_key=#{session[:access_key]} --secret_key=#{session[:secret_key]} --access_token=#{session[:access_token]}")
    s3cmd.run_command
    @buckets = []
    s3cmd.stdout.lines do |line|
      case line
      when /^\s*(.*)\s+s3:\/\/(.*)$/
        @buckets.push({type: :content,  createdon: $1, title: $2})
      end
    end
  end

  def new  # Show the create form
  end

  def create  # Handler the POST (create)
    s3cmd = Mixlib::ShellOut.new("s3cmd mb s3://#{params[:bucket][:title]} --access_key=#{session[:access_key]} --secret_key=#{session[:secret_key]} --access_token=#{session[:access_token]}")
    s3cmd.run_command
    redirect_to buckets_path
  end

  def show
    if params[:method] == 'delete'
      destroy
    else
      render plain: "Displaying Bucket " + params[:id].inspect
    end
  end

  def destroy
    s3cmd = Mixlib::ShellOut.new("s3cmd rb s3://#{params[:id]} --access_key=#{session[:access_key]} --secret_key=#{session[:secret_key]} --access_token=#{session[:access_token]}")
    s3cmd.run_command
    redirect_to buckets_path
  end
end
