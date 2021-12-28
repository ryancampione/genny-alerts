require "functions_framework"
require "base64"
require 'sendgrid-ruby'

include SendGrid

# This function receives a triggered by a
# message posted to a Cloud Pub/Sub topic
FunctionsFramework.cloud_event "send_alert" do |event|
  message_data = Base64.decode64 event.data["message"]["data"]
  # log Pub/Sub message
  FunctionsFramework.logger.info message_data

  # setup email message
  from = Email.new(email: ENV['EMAIL_FROM'])
  to = Email.new(email: ENV['EMAIL_TO'])
  subject = 'Generator Alert!'
  content = Content.new(type: 'text/plain', value: message_data)
  mail = Mail.new(from, subject, to, content)

  # send email using SendGrid API
  sg = SendGrid::API.new(api_key: ENV['SENDGRID_API_KEY'])
  response = sg.client.mail._('send').post(request_body: mail.to_json)
  
  # log email
  FunctionsFramework.logger.info response.status_code
  FunctionsFramework.logger.info response.headers
end
