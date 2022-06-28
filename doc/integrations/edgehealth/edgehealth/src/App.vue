<template>
  <v-app>
    <v-app-bar app color="black" dark>
      <div class="d-flex align-center">
        <a  :to="{path:'/'}" href="/">
          <v-img
          alt="Vuetify Logo"
          class="shrink mr-2"
          contain
          :src="require('./assets/logo2.png')"
          transition="scale-transition"
          width="250"
        />
        </a>

        
      </div>

      <v-spacer></v-spacer>

       <v-menu v-if="!this.$route.meta.showSide" offset-y>
      <template v-slot:activator="{ on, attrs }">
        <v-btn
          
          dark
          v-bind="attrs"
          v-on="on"
          text
        >
          PRODUCT AND SOLUTIONS <v-icon>mdi-chevron-down</v-icon>
        </v-btn>
      </template>
      <v-list>
        <v-list-item
          to=""
        >
          <v-list-item-title>Artificial intelligence</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Robotics</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Precision medicine</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Augmented and virtual reality</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Genomics</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Telemedicine</v-list-item-title>
        </v-list-item>
        
      </v-list>
    </v-menu>


    <v-menu v-if="!this.$route.meta.showSide" offset-y>
      <template v-slot:activator="{ on, attrs }">
        <v-btn
          
          dark
          v-bind="attrs"
          v-on="on"
          text
        >
          SERVICES AND SUPPORT <v-icon>mdi-chevron-down</v-icon>
        </v-btn>
      </template>
      <v-list>
        <v-list-item
          to=""
        >
          <v-list-item-title>3-D printing</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Artificial intelligence</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Robotics</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Precision medicine</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Genomics</v-list-item-title>
        </v-list-item>
        <v-list-item
          to=""
        >
          <v-list-item-title>Telemedicine</v-list-item-title>
        </v-list-item>
        
      </v-list>
    </v-menu>


      <v-btn v-if="!this.$route.meta.showSide" text to="/login">Login</v-btn>
      <v-btn v-if="!this.$route.meta.showSide" text to="/register">Register</v-btn>
      <v-btn v-if="!this.$route.meta.showSide" text to="/dashboard" outlined>Dashboard</v-btn>
      <!-- <v-btn text to="/doctor">Doctor</v-btn>
      <v-btn text to="/radiology">Patient</v-btn> -->
      <v-avatar
      v-if="this.$route.meta.showSide"
                  size="40px"
                  
                  
                  color="blue"
                >
                  <span>A</span>
                  
                  
                </v-avatar>
                <span class="text-caption" v-if="this.$route.meta.showSide" style="margin-left:3px">Admin</span>
                <v-btn icon><v-icon>mdi-logout</v-icon></v-btn>
       <!-- <v-btn></v-btn> -->
    </v-app-bar>
   

    <v-navigation-drawer
    v-if="this.$route.meta.showSide"
      :v-model="true"
      absolute
      style="background-color:#343A40;padding-top:100px;padding-left:3px;padding-right:3px"
      permanent
      
      width="50"
    >

    
        <v-btn icon to="/dashboard"><v-icon  color="white"> mdi-view-dashboard</v-icon></v-btn>
        <hr style="border:grey">
        <br>
        <v-btn to="/doctor" icon><v-icon  color="white">  mdi-doctor</v-icon></v-btn>
        <hr style="border:grey">
        <br>
        <v-btn to="/radiology" icon><v-icon  color="white">  mdi-image-search </v-icon></v-btn>
        <hr style="border:grey">
        <br>
        <v-btn icon><v-icon  color="white">  mdi-account </v-icon></v-btn>
      
      
    </v-navigation-drawer>

    <v-main>
     <router-view></router-view>
    </v-main>
    <v-footer v-if="!this.$route.meta.showSide" style="background-color:white">
      <v-container>
        <v-row align="center" justify="center">
        <v-col cols="4" class="pa-5">
          <p class="text-body-1" style="font-weight:500">CONNECT WITH US</p>
          <v-row>
            <v-icon class="ml-2 mr-3">mdi-facebook</v-icon>
            <v-icon class="mr-3">mdi-twitter</v-icon>
            <v-icon class="mr-3">mdi-linkedin</v-icon>
            <v-icon class="mr-3">mdi-youtube</v-icon>
          </v-row>
          <br>
          <br>
          <br>
          <p class="text-body-1" style="font-weight:500">FIND A LOCATION</p>
          <v-text-field outlined label="Type Location Here" append-icon="mdi-home-search" ></v-text-field>

          <p class="text-body-1" style="font-weight:500">CONTACT US</p>
          <p class="text-caption">Contact us for sales and general inquiries</p>
          <v-btn color="primary" large tile ><span>Contact us</span></v-btn>
        </v-col>
        <v-col cols="8">
            <div style="border-left:0.1px solid grey;height:400px">

                <v-row class="ma-5" >
                  <v-col>
                    <p class="text-body-1" style="font-weight:700">PRODUCTS AND SOLUTIONS</p>
                    <a href="" style="text-decoration:none;color:black">Precision medicine</a><br>
                    <a href="" style="text-decoration:none;color:black">Digital Solutions</a><br>
                    <a href="" style="text-decoration:none;color:black">Telemedicine</a><br>
                    <a href="" style="text-decoration:none;color:black">Robotics</a><br>
                    <a href="" style="text-decoration:none;color:black">Artificial intelligence</a><br>
                    <a href="" style="text-decoration:none;color:black">Augmented and virtual reality</a><br>

                  </v-col>
                  <v-col>
                    <p class="text-body-1" style="font-weight:700">SERVICES AND SUPPORT</p>
                    <a href="" style="text-decoration:none;color:black">Precision medicine</a><br>
                    <a href="" style="text-decoration:none;color:black">Digital Solutions</a><br>
                    <a href="" style="text-decoration:none;color:black">Telemedicine</a><br>
                    <a href="" style="text-decoration:none;color:black">Robotics</a><br>
                    <a href="" style="text-decoration:none;color:black">Artificial intelligence</a><br>
                    <a href="" style="text-decoration:none;color:black">Augmented and virtual reality</a><br>
                  </v-col>
                  <v-col>
                     <p class="text-body-1" style="font-weight:700">ABOUT US</p>
                    <a href="" style="text-decoration:none;color:black">Our Company</a><br>
                    <a href="" style="text-decoration:none;color:black">Careers</a><br>
                    <a href="" style="text-decoration:none;color:black">Media</a><br>
                    <a href="" style="text-decoration:none;color:black">Investors (U.S. site)</a><br>
                    <a href="" style="text-decoration:none;color:black">Supplier (U.S. site)</a><br>
                    <a href="" style="text-decoration:none;color:black">Contact Us</a><br>
                  </v-col>
                  
                </v-row>

            </div>
        </v-col>
      </v-row>
      </v-container>
    </v-footer>
  </v-app>
</template>

<script>


export default {
  name: "App",

  components: {
    
  },

  data: () => ({
    //
    
  }),
};
</script>
