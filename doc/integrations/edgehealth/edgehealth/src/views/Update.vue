<template>
  <div>
      <v-container fill-height fluid>
        <br>

  <v-row align="center"
      justify="center" class="mt-4">
      <v-card height="80vh" width="80vw" >
        <v-subheader   style="background-color:#f1f1f1;" >
          <v-row>
            <v-btn text icon><v-icon size="30">mdi-arrow-left</v-icon></v-btn>
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
      v-model="update.ID"
      outlined
      dense
      type="number"
      label="ID"
      required
    ></v-text-field>

    <v-text-field
      v-model="update.firstname"
      outlined
      dense
      label="First name"
      required
    ></v-text-field>

    <v-text-field
      v-model="update.lastname"
      outlined
      dense
      label="last name"
      required
    ></v-text-field>

    <v-text-field
      v-model="update.speciality"
      outlined
      dense
      label="speciality"
      required
    ></v-text-field>

    <v-text-field
      v-model="update.sabbrev"
      outlined
      dense
      label="sabbrev"
      required
    ></v-text-field>

    <v-text-field
      v-model="update.gender"
      outlined
      dense
      label="Gender"
      required
    ></v-text-field>

     <v-text-field
      v-model="update.office"
      outlined
      dense
      label="Office"
      required
    ></v-text-field>

     <v-text-field
      v-model="update.email"
      outlined
      dense
      label="email"
      required
    ></v-text-field>

     <v-text-field
      v-model="update.phonenum"
      outlined
      dense
      
      label="Phone Number"
      required
    ></v-text-field>

     <v-text-field
      v-model="update.photo"
      outlined
      dense
      label="Photo"
      required
    ></v-text-field>


    <v-btn
      color="success"
      class="mr-4"
      @click="create"
      block
    >
      Update
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
            update:{
                ID:0,
            firstname:'',
            lastname:'',
            speciality:'',
            sabbrev:'',
            gender:'',
            office:'',
            email:'',
            phonenum:'',
            photo:''
            },
            datas:{

            }
        }
    },
    methods:{
         create(){
            // alert("done");
            axios.post('https://vue-health-api.herokuapp.com/update',{
                doctorid:this.update.ID,
                firstname:this.update.firstname,
                lastname:this.update.lastname,
                speciality:this.update.speciality,
                sabbrev:this.update.sabbrev,
                gender:this.update.gender,
                office:this.update.office,
                email:this.update.email,
                phone:this.update.phonenum,
                photo:this.update.photo
            }).then(res=>{
                console.log(res,"success")
                alert("successful")
            }).catch(err=>{
                console.log(err,"error")
                alert("try again")
            })
        }
    },
    mounted(){
        console.log(this.$route.params.id)
        let nana=this.$route.params.id
         axios.get('https://vue-health-api.herokuapp.com/doctors').then(data=>{
            console.log(data.data[0].doctorid)
            for (var i=0;i<data.data.length;i++){
                if(data.data[i].doctorid==nana){
                this.update.ID=data.data[i].doctorid
                this.update.firstname=data.data[i].firstname
                this.update.lastname=data.data[i].lastname
                this.update.speciality=data.data[i].speciality
                this.update.sabbrev=data.data[i].sabbrev
                this.update.gender=data.data[i].gender
                this.update.office=data.data[i].office
                this.update.email=data.data[i].email
                this.update.phonenum=data.data[i].phone
                this.update.photo=data.data[i].photo
        }
      }
    })
    }
}
</script>

<style>

</style>