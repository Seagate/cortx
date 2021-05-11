import React,{ useContext,useEffect,useState } from 'react';
import {Link} from 'react-router-dom';


import './navbar.css'










const Navbar = (props) => {

    return (
        <nav className="navbar navbar-expand-lg fixed-top text-dark portfolio-navbar">
        <div className="container-fluid"><Link className="navbar-brand logo" to="/">Euclid</Link><button data-toggle="collapse" className="navbar-toggler" data-target="#navbarNav"><span className="sr-only">Toggle navigation</span><span className="navbar-toggler-icon"><i class="fa fa-bars" style={{color:'#fff'}}></i></span></button>
            <div className="collapse navbar-collapse"
                id="navbarNav">
                <ul className="nav navbar-nav ml-auto">
                <li className="nav-item" role="presentation"><Link className="nav-link left" to="/about">About</Link></li>
                    <li className="nav-item" role="presentation"><Link className="nav-link left" to="/docs">Documentation</Link></li>
                    <li className="nav-item" role="presentation"><a className="nav-link left" href="/jobs">Github</a></li>
                    <li className="nav-item" role="presentation"><a className="nav-link left" href="/jobs">Demo</a></li>
                    
                    <li className="nav-item" role="presentation"><Link to="/about" class="get-started-btn btn btn-block scrollto">Get Started</Link></li>
                    
                    
                    

                    
                    
                    
                </ul>
            </div>
        </div>
    </nav>
    );
}

export default Navbar;

