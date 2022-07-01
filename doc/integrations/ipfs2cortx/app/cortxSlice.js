import { createSlice } from '@reduxjs/toolkit'

// This is neccesary to reset the state
const initialState = {
  connected: false,
  buckets: new Array(),
  selectedBucket: '',
  bucketFiles: new Array(),
  refreshFiles: true,
  refreshBuckets: true,
}

export const cortxSlice = createSlice({
  name: 'cortx',
  initialState,
  reducers: {
    listBuckets: (state, action) => {
      const { buckets } = action.payload
      state.buckets = buckets
      state.refreshBuckets = false
    },
    selectBucket: (state, action) => {
      const { bucket } = action.payload
      const oldBucket = state.selectedBucket
      if (!state.buckets.includes(bucket)) {
        state.selectedBucket = bucket
      }
      if (bucket != oldBucket) {
        state.bucketFiles = new Array()
        state.refreshFiles = true
      }
    },
    listFile: (state, action) => {
      const { files } = action.payload
      state.bucketFiles = files
      state.refreshFiles = false
    },
    deployedFile: (state) => {
      // Deployed the file to CORTX
      state.refreshFiles = true
    },
    reset: () => initialState,
  },
})

export const { listBuckets, selectBucket, listFile, deployedFile, reset } = cortxSlice.actions

export default cortxSlice.reducer
