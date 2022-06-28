import { createApi, fetchBaseQuery, FetchBaseQueryError } from '@reduxjs/toolkit/query/react'
import all from 'it-all'

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
        console.log('🚀 ~ file: bridgeApi.js ~ line 27 ~ queryFn ~ args', args)
        try {
          const ipfs = window?.ipfsDaemon
          let response = await ipfs.get(cid)
          const file = await all(response)
          console.log('🚀 ~ file: bridgeApi.js ~ line 30 ~ queryFn ~ response', file)

          return { data: file }
        } catch (err) {
          console.log(err)
          return { error: err }
        }
      },
    }),
  }),
})

export const { useLsCidQuery, useGetCidQuery, useLazyGetCidQuery } = bridgeApi
