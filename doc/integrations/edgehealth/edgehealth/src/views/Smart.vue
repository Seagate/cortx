<template>
  <div class="about">
      <v-container fill-height fluid>
        <br>

  <v-row align="center"
      justify="center" class="mt-4">
      <v-card height="80vh" width="80vw" >
        <v-subheader   style="background-color:#f1f1f1;" >
          <v-row>
            <v-btn text to="/dashboard" icon><v-icon size="30" >mdi-arrow-left</v-icon></v-btn>
            <v-spacer></v-spacer>
            <v-chip
      class="ma-2"
      
      color="secondary"
      label
      text-color="white"
    >
      <v-icon left>
         mdi-medical-bag
      </v-icon>
       <v-btn text   to="/doctors/create" color="white" dark>Create</v-btn>
    </v-chip>
            
          </v-row>
      </v-subheader>
      <div class="" style="overflow:hidden;overflow-y:scroll;height:68vh">
        <v-img height="60" src="https://images.unsplash.com/photo-1505751172876-fa1923c5c528?ixid=MnwxMjA3fDB8MHxwaG90by1wYWdlfHx8fGVufDB8fHx8&ixlib=rb-1.2.1&auto=format&fit=crop&w=750&q=80">
          <v-row>
            <v-spacer></v-spacer>
            <p style="margin-top:10px;font-weight:300" class="text-h3">Doctor</p>
            <v-spacer></v-spacer>
          </v-row>
        </v-img>
        <div class="ma-8">

            
           <v-simple-table fixed-header >
    
      <thead>
        <tr>
          <th class="text-left">
            View info
          </th>
          <th class="text-left">
            FirstName
          </th>
          <th class="text-left">
            LastName
          </th>
          
          <th class="text-left">
            Speciality
          </th>
          <th class="text-left">
            gender
          </th>
          <th class="text-left">
            office
          </th>
          <th class="text-left">
            Email
          </th>
          <th class="text-left">
            phonenum
          </th>
          
         
        </tr>
      </thead>
      <tbody>
          <!-- <tr>
              
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="ID"></v-text-field>
              </td>
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="Request"></v-text-field>
              </td>
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="Response"></v-text-field>
              </td>
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="Skill"></v-text-field>
              </td>
              <td>
                  <v-text-field  style="margin-top:10px;margin-bottom:-15px"  dense type="text" outlined placeholder="Score"></v-text-field>
              </td>
              
              
          </tr> -->
        <tr
          v-for="item in logs"
          :key="item.name"
          
        >
          
          <td><v-btn icon :to="{path:`/doctor/${item.firstname.toLowerCase()}-${item.lastname.toLowerCase()}-${item.sabbrev.toLowerCase()}-${item.doctorid}`}"><v-icon>mdi-account-box-multiple-outline</v-icon></v-btn></td>
          <td>{{ item.firstname }}</td>
          <td>{{ item.lastname }}</td>
          
          <td>{{ item.speciality }}</td>
          <td>{{ item.gender }}</td>
           <td>{{ item.office }}</td>
           <td>{{ item.email }}</td>
           <td>{{ item.phone }}</td>
          
        </tr>
      </tbody>
   
  </v-simple-table>

          </div>
      </div>
      </v-card>
  </v-row>
      </v-container>
      </div>
</template>

<script>
import axios from 'axios'
  export default {
    data () {
      return {
        
        logs:[]
      }
    },
    methods:{

    },
    mounted(){
      axios
      .get('https://vue-health-api.herokuapp.com/doctors')
      .then(response => {
        this.logs = response.data
        
        console.log(response.data)
      }).catch(err=>{
        console.log(err)
      })
    }
  }
</script>

<style>

</style>