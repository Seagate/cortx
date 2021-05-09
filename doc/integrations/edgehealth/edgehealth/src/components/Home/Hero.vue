<template>
  <div>
      <v-row class="hero-img" >
          <v-col cols="12" sm="12" md="6" xl="6" lg="6">
            <p style="font-size:70px" class="ml-3">Jefferson Health</p>
            <p class="ml-3" style="font-size:50px;font-weight:bold;margin-top:-40px  ">We Improve Lives</p>

            <p class="ma-5">
                Now more than ever, EdgeHealth is improving lives through exceptional care in the Greater Philadelphia area and South Jersey. With 9 nationally ranked specialties, EdgeHealth continues to top U.S. News & World Report’s list of best hospitals in the area.
            </p>
           <div class="ml-3">
                <p style="font-style:italic" class="">Improving Lives Through Our Attention to Safety</p>
            <p class=" ">We’ve cared for more COVID-19 patients than any other system in the Philadelphia area.</p>
            <p style="font-weight:bold" class=""><a href="" style="text-decoration:none;color:grey">Learn how we are keeping you safe during your visit. </a></p>
            <p class="" style="font-size:23px;font-weight:bold">Improving Lives throughout the Region</p>
            <p>PHILADELPHIA &middot; MONTGOMERY &middot; COUNTYBUCKS &middot; COUNTYSOUTH &middot; JERSEY</p>
           </div>
           <div class="">
            <v-row class="ml-3">
                   <v-col cols="12" sm="12" md="8" xl="8" lg="8">
                       <v-row>
                           <v-btn large :dark="bcolor1==true?true:false" :color="bcolor1==true?'blue':'white'" @click="bcolor1=true;bcolor2=false;bcolor3=false" class="mr-1 text-caption">Site Search</v-btn>

                           <v-btn large :dark="bcolor2==true?true:false" :color="bcolor2==true?'blue':'white'" @click="bcolor1=false;bcolor2=true;bcolor3=false" class="mr-1 text-caption">Find a Doctor</v-btn>

                           <v-btn large :dark="bcolor3==true?true:false" :color="bcolor3==true?'blue':'white'" @click="bcolor1=false;bcolor2=false;bcolor3=true" class="mr-1 text-caption">Find a Location</v-btn>
                       </v-row>
                       
                   </v-col>
                   <v-col cols="12" sm="12" md="4" xl="4" lg="4">
                       
                   </v-col>
               </v-row>
                     
           </div>
           <!-- Search input -->
                <div class="ml-3">
                    <br>
                        <div v-if="bcolor1==true">
                         <v-text-field color="white" background-color="white" outlined placeholder="Search Jefferson Health" append-icon="mdi-magnify"></v-text-field>
                        </div>
                         <div v-else-if="bcolor2==true">
                         <v-text-field color="white" background-color="white" outlined placeholder="Search for a Physician" append-icon="mdi-magnify"></v-text-field>
                        </div>
                         <div v-else-if="bcolor3==true">
                         <v-text-field color="white" background-color="white" outlined placeholder="Search for a Location " append-icon="mdi-magnify" ></v-text-field>
                        </div>
                    </div>
          </v-col>
          <v-col cols="12" sm="12" md="6" xl="6" lg="6">
              
          </v-col>
      </v-row>
  </div>
</template>

<script>
export default {
    data(){
        return{
            bcolor1:true,
            bcolor2:false,
            bcolor3:false
        }
    }
}
</script>

<style>
    .hero-img{
       
   
   color: white;
   
   align-items: center;
    
  background: linear-gradient(to right,#3204fdba, #9907facc), url(https://picsum.photos/1280/853/?random=1) no-repeat  top center;

    }
</style>