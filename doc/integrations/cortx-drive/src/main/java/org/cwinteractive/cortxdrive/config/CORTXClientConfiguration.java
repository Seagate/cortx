package org.cwinteractive.cortxdrive.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import com.amazonaws.auth.AWSStaticCredentialsProvider;
import com.amazonaws.auth.BasicAWSCredentials;
import com.amazonaws.client.builder.AwsClientBuilder.EndpointConfiguration;
import com.amazonaws.regions.Regions;
import com.amazonaws.services.s3.AmazonS3;
import com.amazonaws.services.s3.AmazonS3ClientBuilder;

@Configuration
public class CORTXClientConfiguration {
	
	@Value("${aws.accessKey}")
	private String accessKey;
	
	@Value("${aws.secretKey}")
	private String secretKey;
	
	@Value("${aws.endpointUrl}")
	private String endpointUrl;

	@Bean
	public AmazonS3 cortxS3client() {
		
		System.out.println("Setting up AWS S3 client ..");
		var credentials = new BasicAWSCredentials(accessKey, secretKey);
		
		var endpointConfiguration = new EndpointConfiguration(endpointUrl, "eu-central-1");
		var s3client = AmazonS3ClientBuilder.standard().withCredentials(new AWSStaticCredentialsProvider(credentials))
			// .withRegion(Regions.EU_CENTRAL_1)
			.enablePathStyleAccess()
			.withEndpointConfiguration(endpointConfiguration)
			.build();
				
		return s3client;
	}
	
}
