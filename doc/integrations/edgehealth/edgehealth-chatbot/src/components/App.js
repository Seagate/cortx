import React from 'react';
import {BrowserRouter,Route} from 'react-router-dom'
import Landing from './pages/Landing';
import About from './pages/About';
import Shop from './shop/Shop'

import Header from './Header'
import Chatbot from './chatbot/Chatbot'

const App=()=>{
    return(
        <BrowserRouter>
            <Header />
            <div>
                <Route exact path="/" component={Landing}  />
                <Route exact path="/about" component={About}  />
                <Route exact path="/shop" component={Shop}  />
                <Chatbot />
            </div>
        </BrowserRouter>
    )
}
export default App;