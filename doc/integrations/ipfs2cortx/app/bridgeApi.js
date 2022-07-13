import { createApi, fetchBaseQuery, FetchBaseQueryError } from '@reduxjs/toolkit/query/react'
import all from 'it-all'
import qs from 'qs'

export const bridgeApi = createApi({
  reducerPath: 'bridgeApi',
  baseQuery: fetchBaseQuery({
    baseUrl: '/',
  }),
  tagTypes: ['GET', 'POST'],
  endpoints: (build) => ({
    lsCid: build.query({
      async queryFn(args, _api, _extraOptions, fetch) {
        const { cid } = args
        try {
          const ipfs = window?.ipfsDaemon
          let response = await ipfs.ls(cid)
          const files = await all(response)
          return { data: files }
        } catch (err) {
          console.log(err)
          return { error: err }
        }
      },
    }),
    getCid: build.query({
      async queryFn(args, _api, _extraOptions, fetch) {
        const { cid } = args
        // console.log('🚀 ~ file: bridgeApi.js ~ line 27 ~ queryFn ~ args', args)
        try {
          const ipfs = window?.ipfsDaemon
          let response = await ipfs.get(cid)
          const fileInfo = await all(response)
          return { data: fileInfo }
        } catch (err) {
          console.log(err)
          return { error: err }
        }
      },
    }),
    getBuckets: build.query({
      query: (args) => {
        return `api/s3listBuckets`
      },
    }),
    getBucketFiles: build.query({
      query: (args) => {
        const query = qs.stringify(args)
        return `api/s3listObjects?${query}`
      },
    }),
    postFiles2Bucket: build.mutation({
      query(body) {
        return {
          url: 'api/s3deployObject',
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: body,
        }
      },
    }),
    sayHiJon: build.query({
      query: (args) => {
        const query = qs.stringify(args)
        return `api/hello`
      },
    }),
  }),
})

export const {
  useLsCidQuery,
  useGetCidQuery,
  useLazyGetCidQuery,
  useGetBucketsQuery,
  useLazyGetBucketFilesQuery,
  usePostFiles2BucketMutation,
  useGetBucketFilesQuery,
} = bridgeApi
