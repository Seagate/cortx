require 'mixlib/shellout'
require 'tmpdir'

class ObjectsController < ApplicationController
  def index  # Display all objects
    @objects = []
    @bucket_name = params[:bucket_id]
    # @objects.push({"title" => "Object_name_1"})
    puts "s3cmd ls #{@bucket_name}  --recursive"
    s3cmd = Mixlib::ShellOut.new("s3cmd ls s3://#{@bucket_name}  --recursive --access_key=#{session[:access_key]} --secret_key=#{session[:secret_key]} --access_token=#{session[:access_token]}")
    s3cmd.run_command
    @objects = []
    s3cmd.stdout.lines do |line|
      case line
      when /^\s*DIR\s+(s3:\/\/.*)$/
        @objects.push({type: :commonprefix, title: $2})
      when /^(.*)\s+(\d+)\s+s3:\/\/([^\/]+)\/(.*)$/
        @objects.push({type: :content,  lastmodified: $1, objectsize: $2, bucketname: $3, objectname: $4})
      end
    end
  end

  def create  # Handler the POST (create)
    @bucket_name = params[:bucket_id]
    @object_name = params[:object][:title]

    uploaded_io = params[:object][:s3object]
    upload_file_path = Rails.root.join('public', 'uploads', @object_name)

    File.open(upload_file_path, 'wb') do |file|
      file.write(uploaded_io.read)
    end

    s3cmd = Mixlib::ShellOut.new("s3cmd put #{upload_file_path} s3://#{@bucket_name}/#{@object_name} --access_key=#{session[:access_key]} --secret_key=#{session[:secret_key]} --access_token=#{session[:access_token]}")
    s3cmd.run_command
    redirect_to bucket_objects_path
    # render plain: "s3cmd put #{uploaded_temp_io_path} s3://#{@bucket_name}/#{@object_name}"
  end

  def show
    @bucket_name = params[:bucket_id]
    @object_name = params[:id]
    @object_name += ".#{params[:format]}" if params[:format]
    Rails.logger.info("@object_name = " + @object_name)

    @object = {}
    if params[:method] == 'delete'
      destroy
    else
      # Show treated as Download.
      @object[:bucketname] = @bucket_name
      @object[:objectname] = @object_name

      # Dir.mktmpdir do |current_tmp_dir|
      #   s3cmd = Mixlib::ShellOut.new("s3cmd get s3://#{@bucket_name}/#{@object_name} --access_key=#{session[:access_key]} --secret_key=#{session[:secret_key]} --access_token=#{session[:access_token]}", :cwd => current_tmp_dir)
      #   s3cmd.run_command
      #   send_file "#{current_tmp_dir}/#{@object_name}", :disposition => 'attachment'
      # end

      current_tmp_dir = Dir.tmpdir()

      s3cmd = Mixlib::ShellOut.new("s3cmd get s3://#{@bucket_name}/#{@object_name} --access_key=#{session[:access_key]} --secret_key=#{session[:secret_key]} --access_token=#{session[:access_token]}", :cwd => current_tmp_dir)
      s3cmd.run_command

      filename = File.join(current_tmp_dir, @object_name)
      file = File.open(filename, "rb")
      contents = file.read
      file.close

      File.delete(filename) if File.exist?(filename)

      send_data(contents, :filename => @object_name, :disposition => 'attachment')

      # s3cmd = Mixlib::ShellOut.new("curl -I http://s3.seagate.com/#{@bucket_name}/#{@object_name}")
      # s3cmd.run_command
      # s3cmd.stdout.lines do |line|
      #   case line
      #   when /^Last-Modified:\s*(.*)$/
      #     @object[:lastmodified] = $1
      #   when /^ETag:\s*(.*)$/
      #     @object[:md5] = $1
      #   end
      # end
      # @object = {"title" => "Object_name_2", "description" => "Second Object", "creation_time" => "4-Oct-2015", "md5sum" => "ABCDKHFL"}
      # render plain: "Displaying Object " + s3cmd.stdout
    end
  end

  def destroy
    s3cmd = Mixlib::ShellOut.new("s3cmd del s3://#{@bucket_name}/#{@object_name} --access_key=#{session[:access_key]} --secret_key=#{session[:secret_key]} --access_token=#{session[:access_token]}")
    s3cmd.run_command
    redirect_to bucket_objects_path
  end
end
