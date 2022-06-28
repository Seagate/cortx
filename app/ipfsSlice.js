import { createSlice } from '@reduxjs/toolkit'

// remove idx from array
function arrRemoveByElement(arr, value) {
  return arr.filter(function (ele) {
    return ele != value
  })
}

// remove idx from array
function arrRemoveByIdx(arr, idx) {
  return arr.filter(function (ele, i) {
    return i != idx
  })
}

// This is neccesary to reset the state
const initialState = {
  cid: null,
  selectedIdx: new Array(),
  selectedFiles: new Array(),
  deployed: false,
}

export const ipfsReduxSlice = createSlice({
  name: 'ipfsRedux',
  initialState,
  reducers: {
    setCid: (state, action) => {
      // Get the file information from the current cid.
      const { cid } = action.payload
      state.cid = cid
    },
    selectFile: (state, action) => {
      const { idx, file } = action.payload
      if (!state.selectedIdx.includes(idx)) {
        state.selectedIdx.push(idx)
        console.log('🚀 ~ file: ipfsSlice.js ~ line 31 ~ selectedIdx', state.selectedIdx.length)
        state.selectedFiles.push(file)
      }
    },
    unselectFile: (state, action) => {
      const { idx } = action.payload
      console.log('🚀 ~ file: ipfsSlice.js ~ line 52 ~ idx', idx)

      if (state.selectedIdx.includes(idx)) {
        const file_idx = state.selectedIdx.indexOf(idx)
        state.selectedFiles = arrRemoveByIdx(state.selectedFiles, file_idx)
        state.selectedIdx = arrRemoveByElement(state.selectedIdx, idx)
        console.log('Removed file: ', idx)
      }
    },
    deployFile: (state) => {
      // Deploy the file to CORTX
      if (true) {
        state.deployed = true
      }
    },
    reset: () => initialState,
  },
})

export const { setCid, selectFile, unselectFile, deployFile, reset } = ipfsReduxSlice.actions

export default ipfsReduxSlice.reducer
