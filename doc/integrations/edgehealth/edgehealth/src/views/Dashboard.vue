<template>
  <div style="background-color:#F5F5F5;height:89vh;width:100vw;display: table-cell; vertical-align: middle;">
      <v-container style="">
        <v-row>
          <v-spacer></v-spacer>
          <v-chip
      class="ma-2"
      
      color="cyan"
      label
      text-color="white"
    >
      <v-icon left>
         mdi-medical-bag
      </v-icon>
      <a target="_blank" style="text-decoration:none;color:white"  href="https://edgehealthradiology.herokuapp.com/">Radiology Files</a>
    </v-chip>
          
        </v-row>
          <v-row align="center" justify="center" style="">
              
              <v-col cols="4">
            <v-card
            hover
            to="/doctor"
            elevation="4"
            style="background-color:#343A40;color:white"
            >
                <v-img
      height="50"
      src="https://images.unsplash.com/photo-1505751172876-fa1923c5c528?ixid=MnwxMjA3fDB8MHxwaG90by1wYWdlfHx8fGVufDB8fHx8&ixlib=rb-1.2.1&auto=format&fit=crop&w=750&q=80"
    ></v-img>
              <div class="pa-4">
                  <p style="font-weight:600;margin-bottom:-3px" class="text-body-1"> Doctors<v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
              </div>
            </v-card>
              
          </v-col>
          <v-col cols="4">
            <v-card
            hover
            to="/radiology"
            elevation="4"
            style="background-color:#343A40;color:white"
            >
                <v-img
      height="50"
      src="https://images.unsplash.com/photo-1584555684040-bad07f46a21f?ixid=MnwxMjA3fDB8MHxwaG90by1wYWdlfHx8fGVufDB8fHx8&ixlib=rb-1.2.1&auto=format&fit=crop&w=421&q=80"
    ></v-img>
              <div class="pa-4">
                  <p style="font-weight:600;margin-bottom:-3px" class="text-body-1"> Radiology<v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
              </div>
            </v-card>
              
          </v-col>
           <v-col cols="4">
            <v-card
            hover
            to="/log"
            elevation="4"
            style="background-color:#343A40;color:white"
            >
                <v-img
      height="50"
      src="https://images.unsplash.com/photo-1522442676585-c751dab71864?ixid=MnwxMjA3fDB8MHxwaG90by1wYWdlfHx8fGVufDB8fHx8&ixlib=rb-1.2.1&auto=format&fit=crop&w=750&q=80"
    ></v-img>
              <div class="pa-4">
                  <p style="font-weight:600;margin-bottom:-3px" class="text-body-1"> Logs<v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
              </div>
            </v-card>
              
          </v-col>
          
          
          </v-row>
          <v-row align="center" justify="center" >
              
              <v-col cols="4">
            <v-card
            hover
            to="/patient"
            elevation="4"
            style="background-color:#343A40;color:white"
            >
                <v-img
      height="50"
      src="https://images.unsplash.com/photo-1550791871-0bcd47c97881?ixlib=rb-1.2.1&ixid=MnwxMjA3fDB8MHxwaG90by1wYWdlfHx8fGVufDB8fHx8&auto=format&fit=crop&w=750&q=80"
    ></v-img>
              <div class="pa-4">
                  <p style="font-weight:600;margin-bottom:-3px" class="text-body-1"> Patient<v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
              </div>
            </v-card>
              
          </v-col>
          <v-col cols="4">
            <v-card
            hover
            to="/cafeteria"
            elevation="4"
            style="background-color:#343A40;color:white"
            >
                <v-img
      height="50"
      src="https://images.unsplash.com/photo-1568992688527-e1c1894e4e30?ixid=MnwxMjA3fDB8MHxwaG90by1wYWdlfHx8fGVufDB8fHx8&ixlib=rb-1.2.1&auto=format&fit=crop&w=334&q=80"
    ></v-img>
              <div class="pa-4">
                  <p style="font-weight:600;margin-bottom:-3px" class="text-body-1"> Cafeteria<v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
              </div>
            </v-card>
              
          </v-col>
          <v-col cols="4">
            <v-card
            hover
            to="/radiology"
            elevation="4"
            style="background-color:#343A40;color:white"
            >
                <v-img
      height="50"
      src="https://images.unsplash.com/photo-1502740479091-635887520276?ixid=MnwxMjA3fDB8MHxwaG90by1wYWdlfHx8fGVufDB8fHx8&ixlib=rb-1.2.1&auto=format&fit=crop&w=749&q=80"
    ></v-img>
              <div class="pa-4">
                  <p style="font-weight:600;margin-bottom:-3px" class="text-body-1"> Help?<v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
              </div>
            </v-card>
              
          </v-col>
          
          
          </v-row>
          
          
      </v-container>
  </div>
</template>

<script>

export default {

}
</script>

<style>

</style>