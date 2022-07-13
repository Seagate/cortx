package org.cwinteractive.cortxdrive.services;

import java.io.File;
import java.io.FileOutputStream;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpEntity;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpMethod;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Service;
import org.springframework.util.LinkedMultiValueMap;
import org.springframework.util.MultiValueMap;
import org.springframework.util.StreamUtils;
import org.springframework.web.client.RestTemplate;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;

@Service
public class IPFSFileService {
	
	@Autowired
	ObjectMapper objectMapper;

	Logger logger = LoggerFactory.getLogger(IPFSFileService.class);
	
	@Value("${ipfs.addFileUrl}")
	String ipfsAddFileUrl;
	
	@Value("${ipfs.retrieveFileUrl}")
	String ipfsRetrieveFileUrl;
	
	public IPFSFileService() { }
	
	public Map<String,Object> save(File file) throws Exception {
		
		HttpHeaders headers = new HttpHeaders();
		headers.setContentType(MediaType.MULTIPART_FORM_DATA);
		headers.setAccept(List.of(MediaType.APPLICATION_JSON));
		
		MultiValueMap<String, Object> body = new LinkedMultiValueMap<>();
		body.add("file", file);
		
		HttpEntity<MultiValueMap<String, Object>> requestEntity = new HttpEntity<>(body, headers);
		RestTemplate restTemplate = new RestTemplate();
		ResponseEntity<String> response = restTemplate.postForEntity(ipfsAddFileUrl, requestEntity, String.class);
		
		TypeReference<HashMap<String, Object>> typeRef = new TypeReference<HashMap<String, Object>>() {};
		HashMap<String, Object> responseMap = objectMapper.readValue(response.getBody().toString(), typeRef);
		
		logger.info(String.format("SaveFileResult: Response Code: %s, Response text: %s", response.getStatusCode(), response.toString()));
		
		return responseMap;
	
	}
	
	public File download(String cid, String fileName) throws Exception {
		String prefix = fileName.substring(0, fileName.lastIndexOf('.'));
		String suffix = fileName.substring(fileName.lastIndexOf('.'));
		RestTemplate restTemplate = new RestTemplate();
		File downloadedFile = restTemplate.execute(String.format("%s?arg=%s", ipfsRetrieveFileUrl, cid), HttpMethod.GET, null, clientHttpResponse -> {
			File tmpFile = File.createTempFile(prefix, suffix);
			StreamUtils.copy(clientHttpResponse.getBody(), new FileOutputStream(tmpFile));
			return tmpFile;
		});
		
		return downloadedFile;
	}
	
}
