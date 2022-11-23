package org.cwinteractive.cortxdrive.models;

public class StatusMessage {

	private String title;
	private String details;
	private Integer messageType;
	public String getTitle() {
		return title;
	}
	public void setTitle(String title) {
		this.title = title;
	}
	public String getDetails() {
		return details;
	}
	public void setDetails(String details) {
		this.details = details;
	}
	public Integer getMessageType() {
		return messageType;
	}
	public void setMessageType(Integer messageType) {
		this.messageType = messageType;
	}
	public StatusMessage(String title, String details, Integer messageType) {
		super();
		this.title = title;
		this.details = details;
		this.messageType = messageType;
	}
	
	
}
