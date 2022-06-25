import { configureStore } from '@reduxjs/toolkit'
import counterReducer from './counterSlice'
import ipfsReduxReducer from './ipfsSlice'

export default configureStore({
    reducer: {
        counter: counterReducer,
        ipfsRedux: ipfsReduxReducer,
    },
})
