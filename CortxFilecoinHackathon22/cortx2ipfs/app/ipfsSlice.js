import { createSlice } from "@reduxjs/toolkit";


export const ipfsReduxSlice = createSlice({
    name: 'ipfsRedux',
    initialState: {
        CID: '',
        name: '',
        size: '',
        deployed: false,
    },
    reducers: {
        getFile: (state, action) => {
            // Get the name, size and download the file.
            state.CID = action.payload
        },
        deployFile: (state) => {
            // Deploy the file to CORTX
            if (true) {
                state.deployed = true;
            }
        },
        resetFile: (state) => {
            state = state.initialState
        }
    },
})

export const { getFile, deployFile, resetFile } = ipfsReduxSlice.actions

export default ipfsReduxSlice.reducer
