package org.cwinteractive.cortxdrive.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import com.fasterxml.jackson.databind.ObjectMapper;

@Configuration
public class ObjectMapperConfiguration {

	@Bean
	public ObjectMapper objectMapper() {
		return new ObjectMapper();
	}
	
}
