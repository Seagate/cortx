<template>
  <div>
      <v-container fill-height fluid>
        <br>

  <v-row align="center"
      justify="center" class="mt-4">
      <v-card height="80vh" width="80vw" >
        <v-subheader   style="background-color:#f1f1f1;" >
          <v-row>
            <v-btn to="/radiology" text icon><v-icon size="30">mdi-arrow-left</v-icon></v-btn>
          </v-row>
      </v-subheader>
      <div class="mt-3" style="overflow:hidden;overflow-y:scroll;height:68vh">

        <div class="ma-8">
            <v-form
    ref="form"
    v-model="valid"
    lazy-validation
  >
    <v-text-field
      v-model="firstname"
      outlined
      dense
      
      label="First name"
      required
    ></v-text-field>

    <v-text-field
      v-model="lastname"
      outlined
      dense
      label="lastname"
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
      v-model="phonenumber"
      outlined
      dense
      
      label="Phone Number"
      required
    ></v-text-field>

     <label for="">

        <input type="file" id="file" name="filetoupload" ref="file" v-on:change="handleFile"/>
     </label>
  <br>
  <br>

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
          //  file uploads

                        let formData = new FormData();
                      formData.append('filetoupload', this.file);
                      formData.append('firstname',this.firstname);
                      formData.append('lastname',this.lastname);
                      formData.append('email',this.email);
                      formData.append('phone',this.phonenum);
                      
                      console.log(formData)
              
            axios.post('https://vue-health-api.herokuapp.com/api/upload',formData
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