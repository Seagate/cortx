<template>
  <div class="about ma-5" >
      <v-container fill-height >
        <div class="mt-3" style="overflow:hidden;overflow-y:scroll;height:76vh">
        <br>
        <v-row>
          <v-col cols="8">

            <v-row >
              <v-col cols="4">
                <div style="max-height:20vh">
                    <img height="200" width="200" style=" border-radius: 50%;" :src="datas.photo" />
                </div>
              </v-col>
              <v-col cols="8" class="pl-8">
                <p class="text-h3 font-weight-bold">Dr. {{datas.firstname}}  {{datas.lastname}}, {{datas.sabbrev}}</p>
                <v-chip
      class="ma-2 font-weight-bold"
      color="secondary"
    >
      {{datas.speciality}}
    </v-chip>

    <p  class="text-body2 ml-5 font-weight-bold">{{datas.office.toLowerCase()}}</p>
    <hr>
    <br>
    <v-row>
      <v-chip
      class="ma-2 font-weight-bold"
      color="info"
    >
      444-495-4333
    </v-chip>
    <v-chip
      class="ma-2 font-weight-bold"
      color="danger"
    >
      {{datas.email}}
    </v-chip>
    <v-btn text class="mt-1" :to="{path:`/doctor/update/${datas.doctorid}`}"><v-icon size="35">mdi-playlist-edit</v-icon>edit</v-btn>
    </v-row>
              </v-col>
            </v-row>
            <br>
         <v-alert
      border="left"
      colored-border
      color="deep-purple accent-4"
      elevation="2"
    >
      <p class="font-weight-bold text-h6">New office safety precautions</p>
      <p class="font-weight-bold text-body2" style="color:grey;margin-top:-20px">Message from the office of Dr. {{datas.firstname}} {{datas.lastname}}</p>
      <p>“Our biggest concern is patient safety. In order to keep our patients safe, virtual visits are encouraged for anything the physician doesn't need to physically exam you in depth for. Otherwise, if you're coming into the office, we ask you to text the office...”</p>
    </v-alert>
    <br>
      <hr>
      <v-alert
      dense
      type="info"
    >
      Specialties
    </v-alert>
      
      <p class="font-weight-bold">{{datas.speciality}}</p>

       <v-alert
      dense
      type="success"
      text
      
    >
      Practise names
    </v-alert>
      <p class="font-weight-bold">RescueMD Adult Medicine</p>

      <v-alert
      icon="mdi-semantic-web"
      
      text
      type="info"
    >
      Board Certifications
    </v-alert>
     
      <p class="font-weight-bold">American Board of Internal Medicine</p>

     
      
      <v-alert
      icon="mdi-google-analytics"
      text
      type="info"
    >
      Education and training
    </v-alert>
      <p class="font-weight-bold">Medical School - Ross University School of Medicine, Doctor of Medicine</p>
      <p class="font-weight-bold">St. John Hospital and Medical Center, Residency in Internal Medicine</p>

      <v-alert
      icon="mdi-google-analytics"
      text
      type="info"
    >
      Languages spoken
    </v-alert>
      <p class="font-weight-bold">English</p>

      <v-alert
      
      text
      type="success"
    >
      Provider's gender
    </v-alert>
    <p class="font-weight-bold">{{datas.gender}}</p>
          </v-col>
          <v-col cols="4">
            <v-card
            class="pa-5"
            style="background-color:#f1f1f1;height:100vh"
            flat
            >
            <p class="font-weight-bold text-h6">Book an appointment for free</p>

            <div>
              <p>What's your insurance plan?</p>
              
                <v-text-field outlined background-color="white" dense prepend-inner-icon="mdi-abacus"></v-text-field>

            </div>
             <div>
              <p>What is the reason for your visit?</p>
              
                 <v-select
          v-model="e6"
          :items="state"
          :menu-props="{ maxHeight: '400' }"
          label="Select"
          multiple
          outlined
          background-color="white"
          dense
          persistent-hint
        ></v-select>

            </div>
            <div>
              <p>Has the patient seen this doctor before?</p>
              <v-row style="width:100%;" no-gutters>
                <v-col>
                  <div style="background-color:white;height:10vh" class="pa-3">
                    <v-radio
                    
                    :key="n"
                    :label="`No`"
                    :value="n"
                  ></v-radio>
                </div>
                </v-col>
                <v-col>
                  <div style="background-color:white;height:10vh;" class="pa-3">
                    <v-radio
                    
                    :key="n"
                    :label="`Yes`"
                    :value="n"
                  ></v-radio>
                </div>
                </v-col>
              </v-row>
              
            </div>

            <div>
              <p>Choose the type of appointment</p>
               <v-row style="width:100%;" no-gutters>
                <v-col>
                  <div style="background-color:white;height:10vh" class="pa-3">
                    <v-radio
                    
                    :key="n"
                    :label="`No`"
                    :value="n"
                  ></v-radio>
                </div>
                </v-col>
                <v-col>
                  <div style="background-color:white;height:10vh;" class="pa-3">
                    <v-radio
                    :key="n"
                    :label="`Yes`"
                    :value="n"
                  ></v-radio>
                </div>
                </v-col>
              </v-row>
            </div>

            </v-card>
          </v-col>
        </v-row>
            
        </div>
      </v-container>
      </div>
</template>

<script>
import axios from 'axios'
export default {
  data(){
    return {
      datas:{

      },
      state:['injury','surgery']
      
    }
  },
  mounted(){
    let parami=this.$route.params.id;
    console.log(parami)
    let arr=parami.split('-')
    console.log(arr)
    let newinfo=arr[3]
    axios.get('https://vue-health-api.herokuapp.com/doctors').then(data=>{
      console.log(data.data[0].doctorid)
      for (var i=0;i<data.data.length;i++){
        if(data.data[i].doctorid==newinfo){
          this.datas=data.data[i]
        }
      }
    })
    
  }
}
</script>

<style>

</style>