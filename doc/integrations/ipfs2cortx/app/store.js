import { configureStore } from '@reduxjs/toolkit'
import { setupListeners } from '@reduxjs/toolkit/dist/query'
import { bridgeApi } from './bridgeApi'
import ipfsReduxReducer from './ipfsReduxSlice'
import themeSliceReducer from './themeSlice'
import cortxReducer from './cortxSlice'
import { enableMapSet } from 'immer'

export const store = configureStore({
  reducer: {
    ipfsRedux: ipfsReduxReducer,
    cortx: cortxReducer,
    theme: themeSliceReducer,
    [bridgeApi.reducerPath]: bridgeApi.reducer,
  },
  middleware: (getDefaultMiddleware) =>
    getDefaultMiddleware({
      serializableCheck: false,
    }).concat(bridgeApi.middleware),
})

// optional, but required for refetchOnFocus/refetchOnReconnect beaviors
// see `setupListeners` docs - takes an optional callback as the 2nd arg for customization
setupListeners(store.dispatch)
enableMapSet()
