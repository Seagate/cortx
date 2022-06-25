<template>
  <div>
      <v-container fill-height fluid>
        <br>

  <v-row align="center"
      justify="center" class="mt-4">
      <v-card height="80vh" width="80vw" >
        <v-subheader   style="background-color:#f1f1f1;" >
          <v-row>
            <v-btn to="/doctor" text icon><v-icon size="30">mdi-arrow-left</v-icon></v-btn>

          </v-row>
      </v-subheader>
      <div class="mt-3" style="overflow:hidden;overflow-y:scroll;height:68vh">
        
        <div class="ma-8">
            <v-form
    ref="form"
    
    lazy-validation
  >
    <v-text-field
      v-model="ID"
      outlined
      dense
      
      label="ID"
      required
    ></v-text-field>

    <v-text-field
      v-model="firstname"
      outlined
      dense
      label="Firstname"
      required
    ></v-text-field>

    <v-text-field
      v-model="lastname"
      outlined
      dense
      label="last name"
      required
    ></v-text-field>

    <v-text-field
      v-model="speciality"
      outlined
      dense
      label="speciality"
      required
    ></v-text-field>

    <v-text-field
      v-model="sabbrev"
      outlined
      dense
      label="sabbrev"
      required
    ></v-text-field>

    <v-text-field
      v-model="gender"
      outlined
      dense
      label="Gender"
      required
    ></v-text-field>

     <v-text-field
      v-model="office"
      outlined
      dense
      label="Office"
      required
    ></v-text-field>

     <v-text-field
      v-model="email"
      outlined
      dense
      label="email"
      required
    ></v-text-field>

     <v-text-field
      v-model="phonenum"
      outlined
      dense
      
      label="Phone Number"
      required
    ></v-text-field>

      <label for="">

        <input type="file" id="file" name="filetoupload" ref="file" v-on:change="handleFile"/>
     </label>


    <v-btn
      color="success"
      class="mr-4"
      @click="create"
    >
      Register
    </v-btn>
    
  </v-form>
          </div>
      </div>
      </v-card>
  </v-row>
      </v-container>
  </div>
</template>

<script>
import axios from 'axios';
export default {
    data(){
        return {
            ID:0,
            firstname:'',
            lastname:'',
            speciality:'',
            sabbrev:'',
            gender:'',
            office:'',
            email:'',
            phonenum:'',
            photo:'',
            file:''
        }
    },
    methods:{
         create(){
            // alert("done");
            
                        let formData = new FormData();
                      formData.append('filetoupload', this.file);
                      formData.append('doctorID',this.ID);
                      formData.append('firstname',this.firstname);
                      formData.append('lastname',this.lastname);
                      formData.append('speciality',this.speciality);
                      formData.append('sabbrev',this.sabbrev);
                      formData.append('gender',this.gender);
                      formData.append('office',this.office);
                      formData.append('email',this.email);
                      formData.append('phone',this.phonenum);
                      
                      console.log(formData)
              
            axios.post('https://vue-health-api.herokuapp.com/api/doctors',formData
  ).then(res=>{
                console.log(res,"success")
                alert("successful")
            }).catch(err=>{
                console.log(err,"error")
                alert("try again")
            })
        },
        handleFile:function(){
      this.file = this.$refs.file.files[0];
    }
    }
    
}
</script>

<style>

</style>