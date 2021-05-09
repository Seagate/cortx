import React from 'react'

import {Link} from 'react-router-dom'
import { makeStyles } from '@material-ui/core/styles';
import AppBar from '@material-ui/core/AppBar';
//import { createStyles, makeStyles, Theme } from "@material-ui/core/styles";
import Toolbar from '@material-ui/core/Toolbar';
import Typography from '@material-ui/core/Typography';
import Button from '@material-ui/core/Button';
import IconButton from '@material-ui/core/IconButton';
import MenuIcon from '@material-ui/icons/Menu';
import logo from "../images/edgehealth.png";

/* const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    background : {
      bgcolor: '#000000'
    },
    root: {
      flexGrow: 1
    },
    menuButton: {
      marginRight: theme.spacing(2)
    },
    title: {
      flexGrow: 1,
      textAlign: "center"
    },
    logo: {
      maxWidth: 250,
      marginRight: '0px'
    }
  })
); */

const useStyles = makeStyles((theme) => ({
  root: {
    flexGrow: 1,
  },
  abRoot: {
    backgroundColor: "black"
  },
  menuButton: {
    marginRight: theme.spacing(2),
  },
  title: {
    flexGrow: 1,
  },
  logo: {
    maxWidth: 250,
    marginRight: '0px'
  }
}));

const Header=()=>{

    const classes = useStyles();

    return (
        <div >
          <AppBar position="static" classes={{root: classes.abRoot, positionStatic: classes.abStatic }}>
            <Toolbar>
              <IconButton edge="start"  color="inherit" aria-label="menu">
                <MenuIcon />
              </IconButton>
              <img src={logo} alt="Kitty Katty!" className={classes.logo} />
{/*               <Typography variant="h6">Bot</Typography>
              <Button color="inherit" href="/shop">Shop</Button>
              <Button color="inherit" href="/about">About</Button> */}
            </Toolbar>
          </AppBar>
    </div>
    )


}
export default Header

