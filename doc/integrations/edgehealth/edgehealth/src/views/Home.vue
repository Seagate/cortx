<template>
  <div class="home">
    <div :style="`background-image:url(${require('../assets/hero.jpg')})`"  class="img" >
        <v-container >
            <v-row style="display: table-cell; vertical-align: middle;height:70vh">
              <v-col cols="8">
                
                <p style="color:white;font-weight:500" class="text-h2 ">
                  Healthcare: Smart Hospitals and Smart Healthcare
                </p>
              </v-col>
              <v-col cols="4">

              </v-col>
            </v-row>
        </v-container>
    </div>
    <v-container>
      <div>
      <p style="font-weight:500" class="text-h4" >
        Smart Healthcare and Smart Hospitals ─ Helping Patients Heal and Doctors Cure
      </p>
      <br>
      <p style="" class="text-body-1">
        Healthcare organizations and hospitals are under pressure to deliver quality patient care without compromising clinical outcomes and operational efficiency. Coping with epidemic outbreaks, ensuing patient safety, keeping on top of technology advances, and caring for a rapidly aging population are some common challenges. 
      </p>
      <p style="" class="text-body-1">
        A hospital can be a complex integration of clinical, information technology, and building systems. An optimized, integrated infrastructure streamlines communications and workflows. It’s an investment that enhances patient care, staff satisfaction and the overall quality of your organization — while reduces costs.
      </p>
    </div>
     
    <div>
      <p></p>
    </div>
    <br>
     <v-container>
      <v-row align="center">
        <v-col cols="6">
          <v-img :src="require('../assets/imgg.webp')">

          </v-img>
        </v-col>
        <v-col cols="6">
          <p class="text-h3" style="font-weight:400">Smart Hospitals and Healthcare Facilities</p>
          <p class="text-h6" style="font-weight:200">With Internet of things (IoT), artificial intelligence (AI) and other rapidly developing technologies, hospitals and healthcare facilities can be truly intelligent, able to sense the needs of the users and respond accordingly.</p>
          <v-btn color="primary" tile x-large>Download Whitepaper</v-btn>
        </v-col>
      </v-row>
    </v-container>
    <br>
    <div>
      <v-row>
          <v-col cols="3">
            <v-card
            class="pa-4"
            elevation="4"
            >
              <p style="font-weight:600;margin-bottom:-3px" class="text-body-1">Augmented and virtual reality <v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab </p>
            </v-card>
              
          </v-col>
          <v-col cols="3">
            <v-card
            class="pa-4"
            elevation="4"
            >
              <p style="font-weight:600;margin-bottom:-3px" class="text-body-1">Artificial intelligence <v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
            </v-card>
              
          </v-col>
          <v-col cols="3">
            <v-card
            class="pa-4"
            elevation="4"
            >
              <p style="font-weight:600;margin-bottom:-3px" class="text-body-1">Robotics <v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
            </v-card>
              
          </v-col>
          <v-col cols="3">
            <v-card
            class="pa-4"
            elevation="4"
            >
              <p style="font-weight:600;margin-bottom:-3px" class="text-body-1">Telemedicine <v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
            </v-card>
              
          </v-col>
      </v-row>
      <v-row>
          <v-col cols="3">
            <v-card
            class="pa-4"
            elevation="4"
            >
              <p style="font-weight:600;margin-bottom:-3px" class="text-body-1">Genomics <v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto beatae tempore.</p>
            </v-card>
              
          </v-col>
          <v-col cols="3">
            <v-card
            class="pa-4"
            elevation="4"
            >
              <p style="font-weight:600;margin-bottom:-3px" class="text-body-1">3-D printing <v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
            </v-card>
              
          </v-col>
          <v-col cols="3">
            <v-card
            class="pa-4"
            elevation="4"
            >
              <p style="font-weight:600;margin-bottom:-3px" class="text-body-1">Precision medicine <v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab </p>
            </v-card>
              
          </v-col>
          <v-col cols="3">
            <v-card
            class="pa-4"
            elevation="4"
            >
              <p style="font-weight:600;margin-bottom:-3px" class="text-body-1">Smart Buildings <v-icon>mdi-menu-right</v-icon></p>
              <p class="text-caption">Lorem ipsum dolor sit amet consectetur adipisicing elit. Aspernatur iusto ab beatae tempore.</p>
            </v-card>
              
          </v-col>
      </v-row>

    </div>
   
    <br>
    
    </v-container>
    <div :style="`background-image:url(${require('../assets/hero.jpg')})`">
      <v-container>
        <section>
        <v-row class="ma-5">
          <v-col cols="6" class="">
            <p style="font-size:23px;color:white" class=" ">We are here to help you tailor solutions that meet the unique needs of your healthcare facilities and optimize patients outcome</p>
            <!-- <br> -->
            <v-btn color="primary" large tile>Contact us</v-btn>
          </v-col>
          <v-col cols="6">
            
          </v-col>
        </v-row>
      </section>
      </v-container>
    </div>
    
  </div>
    
</template>

<script>
// @ is an alias to /src


export default {
  name: "Home",
  components: {
    
  },
};
</script>
<style>
.img {
  min-height:70vh;
  box-shadow:inset 0 0 0 2000px rgba(0, 0, 0, 0.534);
  background-size:cover;
  
}
</style>
