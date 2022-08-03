import axios from "axios";
const BASE_URL = "http://localhost:3001";

const instance = axios.create({
  baseURL: BASE_URL,
  timeout: -1,
  headers: {},
});

export const getObject = (bucket, objectId) => {
  const url = `/object/${bucket}/${objectId}`
  return instance.get(url)
};

export const listObjects = (bucket) => {
  const url = `/objects/${bucket}`
  return instance.get(url)
};

export const listBuckets = () => {
  const url = `/buckets`
  return instance.get(url)
};

export const putObject = (bucket, file) => {
  const url = `/object/${bucket}`
  const config = { headers: { 'Content-Type': 'multipart/form-data' } };
  let fd = new FormData();
  fd.append('file',file)
  return instance.post(url, fd, config)
};