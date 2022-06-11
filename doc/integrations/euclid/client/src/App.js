import { Fragment } from "react";
import Home from "./components/Home";
import Navbar from "./components/Navbar";
import { ToastContainer, toast } from 'react-toastify';
import 'react-toastify/dist/ReactToastify.css';

import {BrowserRouter as Router,Route,Switch} from 'react-router-dom';
import Documentation from "./components/Documentation";
import About from "./components/About";


function App() {
  return (
    <Router>
    <Fragment>
    <ToastContainer />
      <Navbar/>
      <Switch>
        <Route exact path="/" component={Home}/>
        <Route exact path="/docs" component={Documentation}/>
        <Route exact path="/about" component={About}/>
      </Switch>
      
    </Fragment>
    </Router>
  );
}

export default App;
