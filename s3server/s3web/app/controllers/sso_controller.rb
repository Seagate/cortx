class SsoController < ApplicationController
  skip_before_filter :is_session_valid

  def verify
    @auth_tokens = params
    res = authenticate_user
    if res
      redirect_to buckets_path
    end
  end
end
